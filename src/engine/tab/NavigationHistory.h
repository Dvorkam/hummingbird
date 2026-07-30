#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Hummingbird::Engine {

// A per-tab back/forward stack (story 7.6.1, extended for the History API MVP in
// 9.6.1). Entries are the tab's requested URLs in visit order — full
// navigations, fragment navigations and `history.pushState` all push, so back
// works across every kind of route.
class NavigationHistory {
public:
    struct Entry {
        std::string url;
        // The serialized `history.state` for a pushState/replaceState entry, as
        // JSON. Empty means "no state", which JS sees as null — distinct from the
        // string "null", which a page can legitimately store.
        std::string state;
        // Which loaded document this entry belongs to. Traversal is
        // same-document — and therefore must NOT reload — exactly when this
        // matches the document currently loaded, which is how browsers decide it.
        //
        // A per-entry "was created by pushState" flag is not enough on its own:
        // going Back from a pushState entry lands on the DOCUMENT's own entry,
        // which no pushState created, and reloading there would defeat the API
        // just as surely.
        uint64_t document_id = 0;
        // True for an entry created by pushState/replaceState. Distinguishes a
        // pushed entry that happens to differ only by fragment from a real
        // fragment navigation, so the former restores state via popstate instead
        // of being mistaken for a hash route.
        bool same_document = false;
    };

    // Records a new full-navigation entry. Like a browser, navigating from a
    // mid-stack position discards the forward entries first. Navigating to the
    // exact current URL (e.g. a reload) is a no-op so it does not stack
    // duplicates.
    void push(std::string url, uint64_t document_id = 0) {
        if (current_ >= 0 && entries_[static_cast<size_t>(current_)].url == url) {
            return;
        }
        push_entry(Entry{std::move(url), {}, document_id, false});
    }

    // `history.pushState`. Deliberately does NOT dedupe on an identical URL: a
    // page may push the same address twice with different state, and browsers
    // give it two entries.
    void push_same_document(std::string url, std::string state, uint64_t document_id) {
        push_entry(Entry{std::move(url), std::move(state), document_id, true});
    }

    // `history.replaceState`. Overwrites the current entry in place, leaving the
    // forward tail alone — replacing is not navigating.
    void replace_current(std::string url, std::string state, uint64_t document_id) {
        if (current_ < 0) {
            push_entry(Entry{std::move(url), std::move(state), document_id, true});
            return;
        }
        auto& entry = entries_[static_cast<size_t>(current_)];
        entry.url = std::move(url);
        entry.state = std::move(state);
        entry.document_id = document_id;
        entry.same_document = true;
    }

    bool can_go_back() const { return current_ > 0; }
    bool can_go_forward() const { return current_ >= 0 && current_ + 1 < static_cast<int>(entries_.size()); }

    // Move the cursor and return the now-current entry. Precondition: the
    // matching can_go_* is true.
    const Entry& go_back() { return entries_[static_cast<size_t>(--current_)]; }
    const Entry& go_forward() { return entries_[static_cast<size_t>(++current_)]; }

    // The entry the cursor is on, or nullptr when the stack is empty.
    const Entry* current() const {
        if (current_ < 0) return nullptr;
        return &entries_[static_cast<size_t>(current_)];
    }

    void clear() {
        entries_.clear();
        current_ = -1;
    }

    size_t size() const { return entries_.size(); }

private:
    void push_entry(Entry entry) {
        entries_.resize(static_cast<size_t>(current_ + 1));  // drop the forward tail
        entries_.push_back(std::move(entry));
        current_ = static_cast<int>(entries_.size()) - 1;
    }

    std::vector<Entry> entries_;
    int current_ = -1;  // index into entries_, or -1 when empty
};

}  // namespace Hummingbird::Engine
