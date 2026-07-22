#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Core {

// One origin's Web Storage map: the backing store behind a single localStorage
// or sessionStorage object (story 8.2.1). Pure data — no JS, no origin logic,
// no persistence — so it is exhaustively testable on its own. The binding
// (8.2.2) supplies the origin and turns a rejected write into a thrown
// QuotaExceededError; persistence (8.2.2) serializes it.
//
// Web Storage is a string->string map with three properties this type enforces:
//   * keys and values are always strings (the JS binding coerces before it gets
//     here, so a caller cannot smuggle a non-string in);
//   * insertion order is preserved, because key(index) indexes into it and a
//     page may iterate 0..length;
//   * it is quota-bounded, and an over-quota set is REFUSED, not truncated or
//     silently dropped — the spec requires a throw, which set() signals by
//     returning false.
//
// There is deliberately no expiry: localStorage lives until removed or cleared
// (see doc/TODOs.md T-STORAGE-EVICTION-1 for why the quota is the only growth
// bound), so load is a plain round-trip with no purge step.
class StorageArea {
public:
    // Per-origin byte budget. 5 MB matches the de-facto browser figure; the
    // story calls for a small cap, and this is the number pages expect.
    static constexpr size_t kDefaultQuotaBytes = 5 * 1024 * 1024;

    StorageArea() = default;
    explicit StorageArea(size_t quota_bytes) : quota_bytes_(quota_bytes) {}

    // getItem: the value for `key`, or nullopt when absent. Distinguishes a
    // stored empty string from a missing key, which `""` could not.
    std::optional<std::string> get_item(std::string_view key) const;

    // setItem. Returns false WITHOUT modifying the store when the change would
    // exceed the quota — the binding turns that into a QuotaExceededError.
    // Overwriting an existing key is measured as the net size change, so
    // shrinking a value never fails.
    bool set_item(std::string_view key, std::string_view value);

    // removeItem: a no-op when the key is absent (per spec).
    void remove_item(std::string_view key);

    // clear: drop every entry.
    void clear();

    // length: the number of stored keys.
    size_t length() const { return entries_.size(); }

    // key(index): the index-th key in insertion order, or nullopt when out of
    // range. A page iterates 0..length over this.
    std::optional<std::string> key_at(size_t index) const;

    // Bytes currently stored (sum of key and value lengths). The quota is
    // measured against this.
    size_t used_bytes() const { return used_bytes_; }
    size_t quota_bytes() const { return quota_bytes_; }

    // Insertion-ordered view, for the persistence layer (8.2.2) to serialize.
    struct Entry {
        std::string key;
        std::string value;
    };
    const std::vector<Entry>& entries() const { return entries_; }

    // Reinstates one entry during load, bypassing the quota (data already on
    // disk is honored even if the cap later shrinks). Returns false and skips
    // the entry only if the key already exists, so a corrupt duplicate cannot
    // shadow a good one.
    bool load_entry(std::string_view key, std::string_view value);

private:
    static size_t entry_size(std::string_view key, std::string_view value) { return key.size() + value.size(); }
    std::vector<Entry>::iterator find(std::string_view key);
    std::vector<Entry>::const_iterator find(std::string_view key) const;

    std::vector<Entry> entries_;
    size_t used_bytes_ = 0;
    size_t quota_bytes_ = kDefaultQuotaBytes;
};

}  // namespace Hummingbird::Core
