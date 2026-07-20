#include "core/BookmarkStore.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace Hummingbird::Core {

namespace {
constexpr std::string_view kSeedUrl = "https://html.duckduckgo.com/html/";
constexpr std::string_view kSeedTitle = "DuckDuckGo (HTML)";

// TSV fields must not contain a tab or newline; collapse them to spaces.
std::string sanitize_field(std::string_view value) {
    std::string out(value);
    for (char& c : out) {
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return out;
}

void append_html_escaped(std::string& out, std::string_view text, bool in_attribute) {
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += in_attribute ? "&quot;" : "\"";
                break;
            default:
                out += c;
                break;
        }
    }
}
}  // namespace

std::filesystem::path BookmarkStore::default_path() {
    if (const char* configured = std::getenv("HB_BOOKMARKS_FILE"); configured && configured[0]) {
        return std::filesystem::path(configured);
    }
    return Utils::resolve_asset_path("assets/config/bookmarks.tsv");
}

BookmarkStore::BookmarkStore(std::filesystem::path path) : path_(std::move(path)) {
    load();
}

void BookmarkStore::load() {
    entries_.clear();
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        // First run: seed with the reference page so the list is never empty.
        entries_.push_back({std::string(kSeedUrl), std::string(kSeedTitle)});
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto tab = line.find('\t');
        std::string url = line.substr(0, tab);
        std::string title = tab == std::string::npos ? url : line.substr(tab + 1);
        if (!url.empty()) entries_.push_back({std::move(url), std::move(title)});
    }
    if (entries_.empty()) {
        entries_.push_back({std::string(kSeedUrl), std::string(kSeedTitle)});
    }
}

bool BookmarkStore::add(std::string url, std::string title) {
    if (url.empty()) return false;
    if (title.empty()) title = url;
    for (auto& entry : entries_) {
        if (entry.url == url) {
            entry.title = sanitize_field(title);  // refresh the title in place
            return false;
        }
    }
    entries_.push_back({std::move(url), sanitize_field(title)});
    return true;
}

void BookmarkStore::save() const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    if (!file) {
        HB_LOG_WARN("[bookmarks] could not write " << path_.string());
        return;
    }
    for (const auto& entry : entries_) {
        file << entry.url << '\t' << sanitize_field(entry.title) << '\n';
    }
}

std::string BookmarkStore::render_html() const {
    // Title and URL are block elements stacked per row (no <li> markers, no
    // reliance on inter-inline whitespace) so the page renders cleanly.
    std::string out;
    out +=
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>Bookmarks</title>"
        "<style>"
        "body{font-family:sans-serif;margin:0;padding:24px 32px;color:#1d2433;background:#ffffff}"
        "h1{font-size:22px;margin:0 0 16px 0}"
        ".bm{display:block;padding:10px 2px;border-bottom:1px solid #ededed}"
        ".bm a{display:block;color:#1a56db;text-decoration:none;font-size:16px}"
        ".bm .u{display:block;color:#6b7280;font-size:13px;margin-top:2px}"
        "</style></head><body><h1>Bookmarks</h1>";
    for (const auto& entry : entries_) {
        out += "<div class=\"bm\"><a href=\"";
        append_html_escaped(out, entry.url, /*in_attribute*/ true);
        out += "\">";
        append_html_escaped(out, entry.title, false);
        out += "</a><span class=\"u\">";
        append_html_escaped(out, entry.url, false);
        out += "</span></div>";
    }
    out += "</body></html>";
    return out;
}

}  // namespace Hummingbird::Core
