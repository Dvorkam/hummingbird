#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Core {

struct Bookmark {
    std::string url;
    std::string title;
};

// A tiny file-backed bookmark list (story 7.6.2): URL + title, no folders,
// favicons, sync, or web-visible API. Shared by the app (adds via Ctrl+D) and the
// built-in network (renders the about:bookmarks page), decoupled through the file.
// A fresh store (no file yet) is seeded with the DuckDuckGo HTML endpoint so there
// is always something to click.
class BookmarkStore {
public:
    // Loads from `path` (seeding when the file is absent).
    explicit BookmarkStore(std::filesystem::path path);
    // Loads from default_path().
    BookmarkStore() : BookmarkStore(default_path()) {}

    // HB_BOOKMARKS_FILE if set, else a file under the asset/config dir.
    static std::filesystem::path default_path();

    const std::vector<Bookmark>& entries() const { return entries_; }

    // Adds a bookmark. Deduped by URL: an existing URL refreshes its title in
    // place and returns false; a new URL is appended and returns true.
    bool add(std::string url, std::string title);

    // Writes the list back to the file (best-effort; logs on failure).
    void save() const;

    // The about:bookmarks page: a plain HTML list of links rendered through the
    // engine's own pipeline.
    std::string render_html() const;

private:
    void load();

    std::filesystem::path path_;
    std::vector<Bookmark> entries_;
};

}  // namespace Hummingbird::Core
