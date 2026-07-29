#include "core/net/HttpCache.h"

#include <algorithm>

#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

namespace {

// Headers a 304 must not carry over onto the stored response. RFC 9111 §4.3.4
// says a 304's headers update the stored ones, but these describe the 304
// itself — copying its `Content-Length: 0` onto a revived 200 would claim the
// body we are about to serve is empty.
bool is_hop_or_body_header(std::string_view name) {
    const std::string lower = Utils::to_lower(name);
    return lower == "content-length" || lower == "transfer-encoding" || lower == "connection" ||
           lower == "keep-alive" || lower == "trailer" || lower == "upgrade";
}

}  // namespace

size_t HttpCache::Entry::footprint() const {
    size_t total = body.size() + url.size() + method.size() + etag.size() + last_modified.size();
    for (const auto& field : headers.fields()) {
        total += field.name.size() + field.value.size();
    }
    return total;
}

std::chrono::seconds HttpCache::Entry::current_age(CacheTime now) const {
    auto resident = std::chrono::seconds(0);
    if (now > received_at) {
        resident = std::chrono::duration_cast<std::chrono::seconds>(now - received_at);
    }
    return initial_age + resident;
}

bool HttpCache::Entry::is_fresh(CacheTime now) const {
    if (always_revalidate) return false;
    return current_age(now) < lifetime;
}

HttpCache::Entry* HttpCache::find(std::string_view method, std::string_view url) {
    for (auto& entry : entries_) {
        if (entry.url == url && Utils::equals_ignore_case(entry.method, method)) {
            return &entry;
        }
    }
    return nullptr;
}

HttpCache::Lookup HttpCache::lookup(std::string_view method, std::string_view url, const HttpHeaders& request_headers,
                                    CacheTime now) {
    // Unused until 9.3.2 keys on it. Taken now so the signature does not have to
    // change under every caller when it does.
    (void)request_headers;

    std::lock_guard<std::mutex> lg(mutex_);
    Entry* entry = find(method, url);
    if (!entry) {
        ++stats_.misses;
        return {};
    }

    entry->last_access = now;

    Lookup result;
    result.etag = entry->etag;
    result.last_modified = entry->last_modified;
    result.age = entry->current_age(now);

    if (!entry->is_fresh(now)) {
        // Stale with no way to ask "is it still good?" is the one case where the
        // entry is worthless: an unconditional refetch would replace it anyway,
        // and keeping it around only costs memory.
        if (entry->etag.empty() && entry->last_modified.empty()) {
            bytes_ -= entry->footprint();
            entries_.erase(entries_.begin() + (entry - entries_.data()));
            ++stats_.misses;
            return {};
        }
        result.outcome = Outcome::MustRevalidate;
        ++stats_.revalidations;
        return result;
    }

    result.outcome = Outcome::Fresh;
    result.status = entry->status;
    result.headers = entry->headers;
    result.body = entry->body;
    ++stats_.hits;
    return result;
}

Storability HttpCache::store(std::string_view method, std::string_view url, long status,
                             const HttpHeaders& request_headers, const HttpHeaders& response_headers, std::string body,
                             CacheTime now) {
    const Storability verdict = storability(method, status, request_headers, response_headers);
    if (verdict != Storability::Storable) {
        return verdict;
    }

    Entry candidate;
    candidate.method = Utils::to_upper(method);
    candidate.url = std::string(url);
    candidate.status = status;
    candidate.headers = response_headers;
    candidate.body = std::move(body);
    candidate.received_at = now;
    candidate.last_access = now;

    const Freshness freshness = compute_freshness(response_headers, now);
    candidate.lifetime = freshness.lifetime;
    candidate.initial_age = freshness.initial_age;
    candidate.always_revalidate = freshness.always_revalidate;

    const Validators validators = extract_validators(response_headers);
    candidate.etag = validators.etag;
    candidate.last_modified = validators.last_modified;

    // A response that is stale on arrival AND has no validator can never be
    // reused: it would fail every freshness check and offer nothing to
    // revalidate with. Storing it would be pure memory cost.
    if (candidate.lifetime.count() <= 0 && !validators.any()) {
        return Storability::NothingToReuse;
    }

    const size_t footprint = candidate.footprint();
    if (footprint > kMaxEntryBytes) {
        // One oversized response must not be able to flush everything else out.
        return Storability::TooLarge;
    }

    std::lock_guard<std::mutex> lg(mutex_);
    if (Entry* existing = find(method, url)) {
        bytes_ -= existing->footprint();
        entries_.erase(entries_.begin() + (existing - entries_.data()));
    }
    evict_for(footprint);
    bytes_ += footprint;
    entries_.push_back(std::move(candidate));
    ++stats_.stores;
    stats_.bytes = bytes_;
    stats_.entries = entries_.size();
    return Storability::Storable;
}

std::optional<HttpCache::Lookup> HttpCache::refresh_from_not_modified(std::string_view method, std::string_view url,
                                                                      const HttpHeaders& request_headers,
                                                                      const HttpHeaders& not_modified_headers,
                                                                      CacheTime now) {
    (void)request_headers;

    std::lock_guard<std::mutex> lg(mutex_);
    Entry* entry = find(method, url);
    if (!entry) {
        return std::nullopt;
    }

    // RFC 9111 §4.3.4: the 304's headers update the stored ones. This is how a
    // server extends a response's life ("still good, and now for another hour")
    // without resending it, so skipping it would make every later use revalidate
    // again immediately.
    for (const auto& field : not_modified_headers.fields()) {
        if (is_hop_or_body_header(field.name)) continue;
        entry->headers.set(field.name, field.value);
    }

    const Freshness freshness = compute_freshness(entry->headers, now);
    entry->received_at = now;
    entry->last_access = now;
    entry->lifetime = freshness.lifetime;
    entry->always_revalidate = freshness.always_revalidate;
    // The revalidated response was generated NOW as far as this cache is
    // concerned, so any Age the CDN reported on the 304 applies from here.
    entry->initial_age = freshness.initial_age;

    const Validators validators = extract_validators(entry->headers);
    entry->etag = validators.etag;
    entry->last_modified = validators.last_modified;

    Lookup result;
    result.outcome = Outcome::Fresh;
    result.status = entry->status;
    result.headers = entry->headers;
    result.body = entry->body;
    result.etag = entry->etag;
    result.last_modified = entry->last_modified;
    result.age = entry->current_age(now);
    ++stats_.not_modified;
    return result;
}

void HttpCache::evict_for(size_t incoming) {
    const auto least_recently_used = [](const Entry& a, const Entry& b) { return a.last_access < b.last_access; };
    while (!entries_.empty() && (bytes_ + incoming > kMaxBytes || entries_.size() + 1 > kMaxEntries)) {
        auto victim = std::min_element(entries_.begin(), entries_.end(), least_recently_used);
        bytes_ -= victim->footprint();
        entries_.erase(victim);
        ++stats_.evictions;
    }
}

HttpCache::Stats HttpCache::stats() const {
    std::lock_guard<std::mutex> lg(mutex_);
    Stats snapshot = stats_;
    snapshot.bytes = bytes_;
    snapshot.entries = entries_.size();
    return snapshot;
}

void HttpCache::clear() {
    std::lock_guard<std::mutex> lg(mutex_);
    entries_.clear();
    bytes_ = 0;
    stats_.bytes = 0;
    stats_.entries = 0;
}

}  // namespace Hummingbird::Core
