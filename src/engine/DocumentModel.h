#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/ArenaAllocator.h"
#include "layout/TreeBuilder.h"
#include "style/StyleEngine.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentModel {
public:
    struct ParseResult {
        bool ok = false;
        bool arena_failed = false;
    };

    void reset();
    ParseResult parse_html(std::string_view html);
    void apply_styles(const std::string& css);
    bool build_render_tree();
    std::optional<std::string> build_form_submission_url(const DOM::Element& input, std::string_view base_url) const;

    bool has_dom_tree() const { return static_cast<bool>(dom_tree_); }
    bool has_render_tree() const { return static_cast<bool>(render_tree_); }
    Layout::RenderObject* render_tree() const { return render_tree_.get(); }
    size_t render_tree_children() const;

    const std::vector<std::string>& style_blocks() const { return style_blocks_; }
    const std::vector<std::string>& stylesheet_links() const { return stylesheet_links_; }
    const std::vector<std::string>& image_links() const { return image_links_; }

private:
    static constexpr size_t kDomArenaBlockSize = 2 * 1024 * 1024;
    static constexpr size_t kDomArenaMaxBlocks = 16;

    Css::StyleEngine style_engine_;
    Layout::TreeBuilder tree_builder_;

    Core::ArenaAllocator dom_arena_{kDomArenaBlockSize, kDomArenaMaxBlocks};
    Core::ArenaPtr<DOM::Node> dom_tree_;
    std::unique_ptr<Layout::RenderObject> render_tree_;

    std::vector<std::string> style_blocks_;
    std::vector<std::string> stylesheet_links_;
    std::vector<std::string> image_links_;
};

}  // namespace Hummingbird::Engine
