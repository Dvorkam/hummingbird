#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "html/HtmlParser.h"
#include "layout/TreeBuilder.h"
#include "layout/geometry/PositioningUtils.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::DOM;
using namespace Hummingbird::Layout;

namespace {

std::string read_fixture(const std::string& name) {
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool has_class_token(const Element* element, std::string_view token) {
    if (!element) return false;
    const auto* attr = element->find_attribute("class");
    if (!attr) return false;
    std::string_view classes(*attr);
    size_t pos = 0;
    while (pos < classes.size()) {
        size_t end = classes.find(' ', pos);
        if (end == std::string_view::npos) end = classes.size();
        if (classes.substr(pos, end - pos) == token) return true;
        pos = end + 1;
    }
    return false;
}

struct AbsoluteBox {
    RenderObject* box = nullptr;
    Rect rect;  // absolute coordinates
};

void find_by_class_recursive(RenderObject* node, std::string_view token, float origin_x, float origin_y,
                             AbsoluteBox& out) {
    if (!node || out.box) return;
    const auto* element = dynamic_cast<const Element*>(node->get_dom_node());
    Rect absolute = node->get_rect();
    absolute.x += origin_x;
    absolute.y += origin_y;
    if (has_class_token(element, token)) {
        out.box = node;
        out.rect = absolute;
        return;
    }
    for (const auto& child : node->get_children()) {
        find_by_class_recursive(child.get(), token, absolute.x, absolute.y, out);
    }
}

AbsoluteBox find_by_class(RenderObject* root, std::string_view token) {
    AbsoluteBox out;
    find_by_class_recursive(root, token, 0.0f, 0.0f, out);
    return out;
}

}  // namespace

// Regression harness for the pinned DuckDuckGo HTML homepage snapshot
// (tests/fixtures/ddg). Assertions are deliberately loose: they lock in
// "the layout is not broken" invariants (nothing off-screen, centered-ish
// search block), not pixel-perfect geometry.
TEST(DdgHomeLayoutTest, HomepageSearchBlockIsOnScreenAndCentered) {
    std::string html = read_fixture("ddg/ddg_home.html");
    std::string css = read_fixture("ddg/ddg_home.css");
    ASSERT_FALSE(html.empty()) << "missing fixture ddg/ddg_home.html";
    ASSERT_FALSE(css.empty()) << "missing fixture ddg/ddg_home.css";

    Hummingbird::Core::ArenaAllocator arena(1 << 22);
    Hummingbird::Html::Parser html_parser(arena, html);
    auto parse_result = html_parser.parse();
    ASSERT_NE(parse_result.dom, nullptr);

    Hummingbird::Css::Parser css_parser(css);
    auto sheet = css_parser.parse();
    EXPECT_GT(sheet.rules.size(), 100u) << "DDG stylesheet parsed suspiciously few rules";

    Hummingbird::Css::StyleEngine engine;
    // Desktop viewport: enables DDG's min-width @media rules (flex centering).
    engine.apply(sheet, parse_result.dom.get(), {1024.0f, 768.0f});

    TreeBuilder builder;
    auto render_root = builder.build(parse_result.dom.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    const Rect viewport{0, 0, 1024, 768};
    render_root->layout(context, viewport);
    Positioning::apply_positioning(*render_root, context, viewport);

    AbsoluteBox logo = find_by_class(render_root.get(), "logo-wrap--home");
    AbsoluteBox search = find_by_class(render_root.get(), "search-wrap--home");
    AbsoluteBox content = find_by_class(render_root.get(), "content-wrap--home");
    ASSERT_NE(logo.box, nullptr) << "logo-wrap--home box missing from render tree";
    ASSERT_NE(search.box, nullptr) << "search-wrap--home box missing from render tree";
    ASSERT_NE(content.box, nullptr) << "content-wrap--home box missing from render tree";

    // Nothing hangs off the left edge (the pre-flex failure mode).
    EXPECT_GE(logo.rect.x, 0.0f);
    EXPECT_GE(search.rect.x, 0.0f);

    // The content wrapper is positioned into the vertical band around top: 24%.
    EXPECT_GT(content.rect.y, 768.0f * 0.10f);
    EXPECT_LT(content.rect.y, 768.0f * 0.45f);

    // The logo+search group sits centered-ish: the span from the logo's left
    // edge to the search form's right edge should straddle the viewport center.
    float group_left = std::min(logo.rect.x, search.rect.x);
    float group_right = std::max(logo.rect.x + logo.rect.width, search.rect.x + search.rect.width);
    EXPECT_LT(group_left, 512.0f);
    EXPECT_GT(group_right, 512.0f);
    // And the group is not degenerate or full-bleed.
    EXPECT_GT(group_right - group_left, 200.0f);
    EXPECT_LT(group_right - group_left, 1024.0f);

    // The duck logo is a CSS background image on a.logo_homepage; regression
    // guard for `background: ..., linear-gradient(transparent, transparent)`
    // wiping the image layer.
    AbsoluteBox logo_anchor = find_by_class(render_root.get(), "logo_homepage");
    ASSERT_NE(logo_anchor.box, nullptr);
    const auto* logo_style = logo_anchor.box->get_computed_style();
    ASSERT_NE(logo_style, nullptr);
    ASSERT_TRUE(logo_style->background_image.has_value()) << "logo background image was cleared";
    EXPECT_NE(logo_style->background_image->find("logo_homepage"), std::string::npos);
    EXPECT_GT(logo_anchor.rect.width, 100.0f);
    EXPECT_GT(logo_anchor.rect.height, 100.0f);

    // Search form anatomy: the input must be tall enough to click (its CSS
    // height is ~2.5em) and the submit button must not blanket the page.
    AbsoluteBox input = find_by_class(render_root.get(), "search__input");
    AbsoluteBox button = find_by_class(render_root.get(), "search__button");
    ASSERT_NE(input.box, nullptr);
    ASSERT_NE(button.box, nullptr);
    EXPECT_GT(input.rect.height, 30.0f) << "search input collapsed to a sliver";
    EXPECT_LT(input.rect.height, 80.0f);
    EXPECT_GT(input.rect.width, 300.0f);
    EXPECT_LT(button.rect.width, 120.0f) << "submit button hit-box is oversized";
    EXPECT_LT(button.rect.height, 120.0f);

    // The submit button is absolutely positioned inside the search form
    // (position:relative). It should sit within the form's vertical band, not
    // dangle below it. Diagnostic values are printed so a failure shows where
    // the button actually lands.
    AbsoluteBox form = find_by_class(render_root.get(), "search");
    ASSERT_NE(form.box, nullptr);
    const float form_top = form.rect.y;
    const float form_bottom = form.rect.y + form.rect.height;
    const float button_top = button.rect.y;
    const float button_bottom = button.rect.y + button.rect.height;
    const std::string where = "form=[" + std::to_string(form_top) + ".." + std::to_string(form_bottom) +
                              "] h=" + std::to_string(form.rect.height) + " button=[" + std::to_string(button_top) +
                              ".." + std::to_string(button_bottom) + "] h=" + std::to_string(button.rect.height);
    EXPECT_GE(button_top, form_top - 4.0f) << where;
    EXPECT_LE(button_bottom, form_bottom + 4.0f) << where;
    // The button uses height:auto between top:0/bottom:0, so it should stretch to
    // roughly the form's height, not sit small at the top.
    EXPECT_GE(button.rect.height, form.rect.height * 0.6f) << where;
}
