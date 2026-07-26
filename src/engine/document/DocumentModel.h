#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/utils/CompatibilityWarnings.h"
#include "engine/document/DocumentLinkDiscovery.h"
#include "engine/forms/FormSubmission.h"
#include "layout/TreeBuilder.h"
#include "style/compute/StyleEngine.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class IFontFaceResolver;

class DocumentModel {
public:
    struct ParseResult {
        bool ok = false;
        bool arena_failed = false;
    };

    DocumentModel() = default;
    // Test/advanced hook: override the DOM arena budget (default is the shipping
    // 32 MB) so budget-exhaustion handling can be exercised (T-DOM-2).
    DocumentModel(size_t arena_block_size, size_t arena_max_blocks) : dom_arena_(arena_block_size, arena_max_blocks) {}

    void reset();
    void flush_compatibility_warnings(std::string_view document_url);
    ParseResult parse_html(std::string_view html);
    // `font_resolver`, when non-null, turns parsed @font-face rules into a
    // registry consulted during style compute; remote fonts it could not resolve
    // yet are exposed via font_requests() for the caller to fetch (T-FONT-FACE-1).
    void apply_styles(const std::string& css, const Css::MediaContext& media = {},
                      const IFontFaceResolver* font_resolver = nullptr);
    // Set the `:visited` pseudo-state on anchors whose href (resolved against
    // base_url) is in the visited set. Call before apply_styles (T-HIST-1).
    void mark_visited_links(const std::unordered_set<std::string>& visited_urls, std::string_view base_url);
    // True when any @media rule matches differently under `media` than under
    // the context styles were last applied with (a breakpoint was crossed).
    bool media_conditions_change(const Css::MediaContext& media) const;
    bool build_render_tree();
    std::optional<FormSubmission> build_form_submission(const DOM::Element& input, std::string_view base_url) const;

    bool has_dom_tree() const { return static_cast<bool>(dom_tree_); }
    bool has_render_tree() const { return static_cast<bool>(render_tree_); }
    Layout::RenderObject* render_tree() const { return render_tree_.get(); }
    size_t render_tree_children() const;

    DOM::Node* dom_root() const { return dom_tree_.get(); }
    Core::ArenaAllocator* dom_arena() { return &dom_arena_; }

    const std::vector<std::string>& style_blocks() const { return style_blocks_; }
    // All <script> elements in document order (inline text or external src).
    const std::vector<DocumentScriptRef>& document_scripts() const { return document_scripts_; }
    const std::vector<std::string>& stylesheet_links() const { return stylesheet_links_; }
    const std::vector<std::string>& image_links() const { return image_links_; }
    const std::vector<std::string>& background_image_links() const { return background_image_links_; }
    // Remote @font-face urls discovered at the last apply_styles that still need
    // fetching (populated only when a font resolver is supplied).
    const std::vector<std::string>& font_requests() const { return font_requests_; }

private:
    static constexpr size_t kDomArenaBlockSize = 2 * 1024 * 1024;
    static constexpr size_t kDomArenaMaxBlocks = 16;

    // Reset the arena and parse the built-in "document too large" page so a
    // budget failure shows a user-facing page instead of a blank tab (T-DOM-2).
    bool load_budget_error_page();

    Core::Utils::CompatibilityWarnings compatibility_warnings_;
    Css::StyleEngine style_engine_{&compatibility_warnings_};
    Layout::TreeBuilder tree_builder_;

    Core::ArenaAllocator dom_arena_{kDomArenaBlockSize, kDomArenaMaxBlocks};
    Core::ArenaPtr<DOM::Node> dom_tree_;
    std::unique_ptr<Layout::RenderObject> render_tree_;

    std::vector<Css::MediaCondition> media_conditions_;
    Css::MediaContext applied_media_;

    std::vector<std::string> style_blocks_;
    std::vector<DocumentScriptRef> document_scripts_;
    std::vector<std::string> stylesheet_links_;
    std::vector<std::string> image_links_;
    std::vector<std::string> background_image_links_;
    std::vector<std::string> font_requests_;
};

}  // namespace Hummingbird::Engine
