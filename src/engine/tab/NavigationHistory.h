#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Hummingbird::Engine {

// A per-tab back/forward stack (story 7.6.1). Entries are the tab's requested
// URLs in visit order — full navigations and fragment navigations both push, so
// back works across hash routes too. Chrome-side only; the JS History API (M12)
// is separate.
class NavigationHistory {
public:
    // Records a new entry. Like a browser, navigating from a mid-stack position
    // discards the forward entries first. Navigating to the exact current URL
    // (e.g. a reload) is a no-op so it does not stack duplicates.
    void push(std::string url) {
        if (current_ >= 0 && entries_[static_cast<size_t>(current_)] == url) {
            return;
        }
        entries_.resize(static_cast<size_t>(current_ + 1));  // drop the forward tail
        entries_.push_back(std::move(url));
        current_ = static_cast<int>(entries_.size()) - 1;
    }

    bool can_go_back() const { return current_ > 0; }
    bool can_go_forward() const { return current_ >= 0 && current_ + 1 < static_cast<int>(entries_.size()); }

    // Move the cursor and return the now-current URL. Precondition: the matching
    // can_go_* is true.
    const std::string& go_back() { return entries_[static_cast<size_t>(--current_)]; }
    const std::string& go_forward() { return entries_[static_cast<size_t>(++current_)]; }

    void clear() {
        entries_.clear();
        current_ = -1;
    }

    size_t size() const { return entries_.size(); }

private:
    std::vector<std::string> entries_;
    int current_ = -1;  // index into entries_, or -1 when empty
};

}  // namespace Hummingbird::Engine
