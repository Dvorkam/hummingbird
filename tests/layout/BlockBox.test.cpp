#include "layout/block/BlockBox.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlAttributeNames.h"
#include "layout/TreeBuilder.h"
#include "layout/controls/RenderRule.h"
#include "layout/flow/InlineBox.h"
#include "layout/flow/TextBox.h"
#include "layout/replaced/ObjectFitUtils.h"
#include "layout/replaced/RenderImage.h"
#include "layout/replaced/ReplacedSizingUtils.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "style/types/ComputedStyle.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;
namespace Attr = Hummingbird::Html::AttributeNames;
using Hummingbird::IGraphicsContext;

namespace {
RenderObject* find_by_id(RenderObject* node, std::string_view id) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const Element*>(node->get_dom_node())) {
        if (const auto* attr = element->find_attribute("id")) {
            if (*attr == id) {
                return node;
            }
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_by_id(child.get(), id)) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

TEST(BlockBoxLayoutTest, SimpleStacking) {
    // Create a DOM tree: <body><p/><p/></body>
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto p1 = DomFactory::create_element(arena, "p");
    auto p2 = DomFactory::create_element(arena, "p");
    // Give the paragraphs some fake height for testing layout
    // In a real scenario, this would come from child text nodes or CSS
    p1->set_attribute(Attr::Height, "10");
    p2->set_attribute(Attr::Height, "20");
    dom_root->append_child(std::move(p1));
    dom_root->append_child(std::move(p2));

    // Build the render tree
    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    // Create a dummy layout function for the test that gives a fixed height
    // This is a hack for now. A real implementation would calculate height from children.
    class TestBlockBox : public BlockBox {
    public:
        explicit TestBlockBox(const Node* dom_node) : BlockBox(dom_node) {}
        void layout(IGraphicsContext& context, const Rect& bounds) override {
            BlockBox::layout(context, bounds);
            const auto* element_node = dynamic_cast<const Hummingbird::DOM::Element*>(get_dom_node());
            if (element_node) {
                const auto& attributes = element_node->get_attributes();
                auto it = attributes.find(std::string(Attr::Height));
                if (it != attributes.end()) {
                    m_rect.height = std::stof(it->second);
                }
            }
        }
    };

    // This is also a hack. We can't easily swap the type created by the TreeBuilder.
    // For now, we'll manually create the test objects.
    auto test_render_root = std::make_unique<TestBlockBox>(dom_root.get());
    auto test_p1 = std::make_unique<TestBlockBox>(dom_root->get_children()[0].get());
    auto test_p2 = std::make_unique<TestBlockBox>(dom_root->get_children()[1].get());
    test_render_root->append_child(std::move(test_p1));
    test_render_root->append_child(std::move(test_p2));

    // Layout the tree
    Rect viewport = {0, 0, 800, 600};
    Hummingbird::Test::TestGraphicsContext context;
    test_render_root->layout(context, viewport);

    // Assertions
    const auto& children = test_render_root->get_children();
    ASSERT_EQ(children.size(), 2);

    const auto& rect1 = children[0]->get_rect();
    EXPECT_EQ(rect1.x, 0);
    EXPECT_EQ(rect1.y, 0);
    EXPECT_EQ(rect1.width, 800);
    EXPECT_EQ(rect1.height, 10);

    const auto& rect2 = children[1]->get_rect();
    EXPECT_EQ(rect2.x, 0);
    EXPECT_EQ(rect2.y, 10);  // Should be stacked below the first one
    EXPECT_EQ(rect2.width, 800);
    EXPECT_EQ(rect2.height, 20);

    // The root's height should be the sum of the children's heights
    EXPECT_EQ(test_render_root->get_rect().height, 30);
}

TEST(BlockBoxLayoutTest, InlineBlockShrinksToContent) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto span = DomFactory::create_element(arena, "span");
    auto text = DomFactory::create_text(arena, "Hello");
    span->append_child(std::move(text));

    auto inline_block = InlineBlockBox::create(span.get());
    inline_block->append_child(TextBox::create(dynamic_cast<Text*>(span->get_children()[0].get())));

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 300, 0};
    inline_block->layout(context, bounds);

    EXPECT_LT(inline_block->get_rect().width, bounds.width);
    EXPECT_GT(inline_block->get_rect().width, 0.0f);
}

TEST(BlockBoxLayoutTest, BlockDescendantDoesNotBalloonInlineBlockShrinkToFitWidth) {
    // Regression for T-LAYOUT-SHRINK-TO-FIT-1 (same root cause as
    // T-LAYOUT-TABLE-INTRINSIC-BLOCK-1): measure_inline() lays an inline-block's
    // children out at an oversized probe width. A nested auto-width
    // display:block descendant must not be measured at that stretched width, or
    // the inline-block balloons to ~kInlineAtomicLayoutWidth and shoves any
    // following inline content off-screen.
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto wrapper = DomFactory::create_element(arena, "span");
    wrapper->set_attribute("id", "w");
    auto nested_block = DomFactory::create_element(arena, "div");
    nested_block->append_child(DomFactory::create_text(arena, "x"));
    wrapper->append_child(std::move(nested_block));
    body->append_child(std::move(wrapper));
    body->append_child(DomFactory::create_text(arena, "y"));

    Parser css_parser("body, div, span { margin: 0; padding: 0; } #w { display: inline-block; }");
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto root = builder.build(body.get());
    ASSERT_NE(root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    root->layout(context, viewport);

    auto* wrapper_render = find_by_id(root.get(), "w");
    ASSERT_NE(wrapper_render, nullptr);

    // Shrinks to its (short) text content, not the ~100000px measurement probe.
    EXPECT_LT(wrapper_render->get_rect().width, 200.0f)
        << "inline-block ballooned to " << wrapper_render->get_rect().width;
}

TEST(BlockBoxLayoutTest, FloatLeftShiftsInlineContent) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, "div");
    auto hr = DomFactory::create_element(arena, "hr");
    auto text = DomFactory::create_text(arena, "Hello world");
    root->append_child(std::move(hr));
    root->append_child(std::move(text));

    auto root_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    root->set_computed_style(root_style);

    auto float_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    float_style->float_type = Hummingbird::Css::ComputedStyle::Float::Left;
    float_style->width = Hummingbird::Css::ComputedStyle::LengthValue::from_px(50.0f);
    float_style->height = Hummingbird::Css::ComputedStyle::LengthValue::from_px(10.0f);
    root->get_children()[0]->set_computed_style(float_style);

    auto render_root = BlockBox::create(root.get());
    auto render_rule = RenderRule::create(root->get_children()[0].get());
    auto render_text = TextBox::create(dynamic_cast<Text*>(root->get_children()[1].get()));
    render_root->append_child(std::move(render_rule));
    render_root->append_child(std::move(render_text));

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 200, 0};
    render_root->layout(context, bounds);

    const auto& text_rect = render_root->get_children()[1]->get_rect();
    EXPECT_GE(text_rect.x, 50.0f);
}

TEST(BlockBoxLayoutTest, BlockStaysBesideFloatWhenRoom) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, "div");
    auto img = DomFactory::create_element(arena, "img");
    auto hr = DomFactory::create_element(arena, "hr");
    img->set_attribute(Attr::Width, "88");
    img->set_attribute(Attr::Height, "31");
    root->append_child(std::move(img));
    root->append_child(std::move(hr));

    auto root_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    root->set_computed_style(root_style);

    auto img_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    img_style->float_type = Hummingbird::Css::ComputedStyle::Float::Right;
    root->get_children()[0]->set_computed_style(img_style);

    auto render_root = BlockBox::create(root.get());
    auto render_img = RenderImage::create(dynamic_cast<Element*>(root->get_children()[0].get()));
    auto render_rule = RenderRule::create(root->get_children()[1].get());
    render_root->append_child(std::move(render_img));
    render_root->append_child(std::move(render_rule));

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 200, 0};
    render_root->layout(context, bounds);

    const auto& rule_rect = render_root->get_children()[1]->get_rect();
    EXPECT_FLOAT_EQ(rule_rect.x, 0.0f);
    EXPECT_FLOAT_EQ(rule_rect.y, 0.0f);
    EXPECT_FLOAT_EQ(rule_rect.width, 112.0f);
}

TEST(BlockBoxLayoutTest, FloatImageInsideLinkIsFloated) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, "div");
    auto link = DomFactory::create_element(arena, "a");
    auto img = DomFactory::create_element(arena, "img");
    img->set_attribute(Attr::Width, "88");
    img->set_attribute(Attr::Height, "31");
    link->append_child(std::move(img));
    root->append_child(std::move(link));
    root->append_child(DomFactory::create_text(arena, "Trailing text"));

    auto root_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    root->set_computed_style(root_style);

    auto link_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    link_style->display = Hummingbird::Css::ComputedStyle::Display::Inline;
    root->get_children()[0]->set_computed_style(link_style);

    auto img_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
    img_style->display = Hummingbird::Css::ComputedStyle::Display::Inline;
    img_style->float_type = Hummingbird::Css::ComputedStyle::Float::Right;
    root->get_children()[0]->get_children()[0]->set_computed_style(img_style);

    auto render_root = BlockBox::create(root.get());
    auto render_link = InlineBox::create(dynamic_cast<Element*>(root->get_children()[0].get()));
    auto render_img = RenderImage::create(dynamic_cast<Element*>(root->get_children()[0]->get_children()[0].get()));
    render_link->append_child(std::move(render_img));
    render_root->append_child(std::move(render_link));
    render_root->append_child(TextBox::create(dynamic_cast<Text*>(root->get_children()[1].get())));

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 200, 0};
    render_root->layout(context, bounds);

    const auto& link_rect = render_root->get_children()[0]->get_rect();
    EXPECT_FLOAT_EQ(link_rect.width, 88.0f);
    EXPECT_FLOAT_EQ(link_rect.x, 112.0f);
}

// --- Story 8.5.1: percentage sizing for replaced elements -------------------

TEST(ReplacedSizingTest, PercentWidthResolvesAgainstContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_percent(50.0f);
    ReplacedSizing::IntrinsicSize intrinsic;  // no intrinsic size

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/200.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 100.0f);  // 50% of the 200px containing block
}

TEST(ReplacedSizingTest, PercentHeightResolvesAgainstContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.height = ComputedStyle::LengthValue::from_percent(25.0f);
    ReplacedSizing::IntrinsicSize intrinsic;

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/200.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.height, 100.0f);  // 25% of the 400px containing block
}

TEST(ReplacedSizingTest, PixelWidthIsUnaffectedByContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_px(120.0f);
    ReplacedSizing::IntrinsicSize intrinsic;

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/200.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 120.0f);  // absolute length, no percentage
}

TEST(ReplacedSizingTest, PercentWidthFallsBackToMagnitudeWithoutBasis) {
    // The inline-measure path plumbs no containing block; a percentage keeps the
    // pre-8.5.1 bare-magnitude behavior rather than resolving to 0 or crashing.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_percent(50.0f);
    ReplacedSizing::IntrinsicSize intrinsic;

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/std::nullopt, /*cb_height=*/std::nullopt);
    EXPECT_FLOAT_EQ(size.width, 50.0f);  // bare magnitude fallback
}

TEST(ReplacedSizingTest, PercentWidthDoesNotBalloonAgainstIntrinsicProbe) {
    // The containing-block basis is the ~100000px intrinsic-measurement probe, so
    // a percentage is indefinite: it must not resolve to a probe-sized width (that
    // is the shrink-to-fit ballooning bug for replaced elements).
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_percent(100.0f);
    ReplacedSizing::IntrinsicSize intrinsic{256.0f, 256.0f, true, true};

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/100000.0f, /*cb_height=*/0.0f);
    EXPECT_LT(size.width, 20000.0f);     // did not balloon to the probe width
    EXPECT_FLOAT_EQ(size.width, 100.0f);  // treated as bare magnitude while measuring
}

TEST(ReplacedSizingTest, AspectRatioDerivesAutoHeightFromWidth) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_px(44.0f);  // height auto
    ReplacedSizing::IntrinsicSize intrinsic{256.0f, 256.0f, true, true};

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/400.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 44.0f);
    EXPECT_FLOAT_EQ(size.height, 44.0f);  // 44 / (256/256), not the intrinsic 256
}

TEST(ReplacedSizingTest, AspectRatioDerivesAutoWidthFromHeight) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.height = ComputedStyle::LengthValue::from_px(50.0f);  // width auto
    ReplacedSizing::IntrinsicSize intrinsic{200.0f, 100.0f, true, true};  // ratio 2:1

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/400.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.height, 50.0f);
    EXPECT_FLOAT_EQ(size.width, 100.0f);  // 50 * (200/100)
}

TEST(ReplacedSizingTest, MaxWidthClampFollowsAspectRatio) {
    // The ubiquitous responsive-image pattern: no width/height, max-width:100%.
    // Clamping the width must pull the height along the intrinsic ratio —
    // 1000x500 in a 300px container is 300x150, not 300x500.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.max_width = ComputedStyle::LengthValue::from_percent(100.0f);
    ReplacedSizing::IntrinsicSize intrinsic{1000.0f, 500.0f, true, true};

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/300.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 300.0f);
    EXPECT_FLOAT_EQ(size.height, 150.0f);
}

TEST(ReplacedSizingTest, ClampedSpecifiedWidthRederivesAutoHeight) {
    // An explicit width that max-width then shrinks: the auto height must follow
    // the clamped width, not the pre-clamp one.
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_px(1000.0f);
    style.max_width = ComputedStyle::LengthValue::from_px(300.0f);
    ReplacedSizing::IntrinsicSize intrinsic{1000.0f, 500.0f, true, true};

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/2000.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 300.0f);
    EXPECT_FLOAT_EQ(size.height, 150.0f);
}

TEST(ReplacedSizingTest, BothDimensionsSpecifiedIgnoresAspectRatio) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto img = DomFactory::create_element(arena, "img");
    ComputedStyle style = default_computed_style();
    style.width = ComputedStyle::LengthValue::from_px(88.0f);
    style.height = ComputedStyle::LengthValue::from_px(31.0f);
    ReplacedSizing::IntrinsicSize intrinsic{200.0f, 100.0f, true, true};

    auto size = ReplacedSizing::compute_layout_size(*img, &style, 300.0f, 150.0f, intrinsic,
                                                    /*cb_width=*/400.0f, /*cb_height=*/400.0f);
    EXPECT_FLOAT_EQ(size.width, 88.0f);
    EXPECT_FLOAT_EQ(size.height, 31.0f);  // ratio not applied when both are set
}

TEST(BlockBoxLayoutTest, ReplacedPercentWidthResolvesAgainstContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, "div");
    root->append_child(DomFactory::create_element(arena, "img"));

    root->set_computed_style(std::make_shared<ComputedStyle>(default_computed_style()));
    auto img_style = std::make_shared<ComputedStyle>(default_computed_style());
    img_style->display = ComputedStyle::Display::Block;
    img_style->width = ComputedStyle::LengthValue::from_percent(100.0f);
    root->get_children()[0]->set_computed_style(img_style);

    auto render_root = BlockBox::create(root.get());
    render_root->append_child(RenderImage::create(dynamic_cast<Element*>(root->get_children()[0].get())));

    Hummingbird::Test::TestGraphicsContext context;
    Rect bounds{0, 0, 200, 0};
    render_root->layout(context, bounds);

    // Before 8.5.1 this resolved to the bare magnitude (100px); now it is 100% of
    // the 200px containing block.
    EXPECT_FLOAT_EQ(render_root->get_children()[0]->get_rect().width, 200.0f);
}

// --- Story 8.5.2: object-fit placement -------------------------------------

TEST(ObjectFitTest, FillReturnsContentBoxUnchanged) {
    Rect content{0, 0, 200, 100};
    auto r = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::Fill, content, 100.0f, 100.0f);
    EXPECT_FALSE(r.needs_clip);
    EXPECT_FLOAT_EQ(r.dest.width, 200.0f);
    EXPECT_FLOAT_EQ(r.dest.height, 100.0f);
}

TEST(ObjectFitTest, ContainPreservesRatioAndCenters) {
    Rect content{0, 0, 200, 100};  // wide box, square image
    auto r = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::Contain, content, 100.0f, 100.0f);
    EXPECT_FALSE(r.needs_clip);
    EXPECT_FLOAT_EQ(r.dest.width, 100.0f);   // scaled to fit the 100px height
    EXPECT_FLOAT_EQ(r.dest.height, 100.0f);
    EXPECT_FLOAT_EQ(r.dest.x, 50.0f);        // centered horizontally: (200-100)/2
    EXPECT_FLOAT_EQ(r.dest.y, 0.0f);
}

TEST(ObjectFitTest, CoverFillsBoxAndFlagsClip) {
    Rect content{0, 0, 200, 100};
    auto r = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::Cover, content, 100.0f, 100.0f);
    EXPECT_TRUE(r.needs_clip);               // 200x200 exceeds the 100px-tall box
    EXPECT_FLOAT_EQ(r.dest.width, 200.0f);
    EXPECT_FLOAT_EQ(r.dest.height, 200.0f);
    EXPECT_FLOAT_EQ(r.dest.x, 0.0f);
    EXPECT_FLOAT_EQ(r.dest.y, -50.0f);       // centered vertically: (100-200)/2
}

TEST(ObjectFitTest, NoneUsesIntrinsicSize) {
    Rect content{0, 0, 200, 100};
    auto r = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::None, content, 100.0f, 100.0f);
    EXPECT_FALSE(r.needs_clip);
    EXPECT_FLOAT_EQ(r.dest.width, 100.0f);
    EXPECT_FLOAT_EQ(r.dest.height, 100.0f);
    EXPECT_FLOAT_EQ(r.dest.x, 50.0f);
}

TEST(ObjectFitTest, ScaleDownNeverScalesUp) {
    // Small box, big image -> behaves like contain (scale down).
    Rect small{0, 0, 50, 50};
    auto down = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::ScaleDown, small, 100.0f, 100.0f);
    EXPECT_FLOAT_EQ(down.dest.width, 50.0f);
    EXPECT_FLOAT_EQ(down.dest.height, 50.0f);

    // Big box, small image -> behaves like none (no upscaling).
    Rect big{0, 0, 200, 200};
    auto up = ObjectFitUtils::compute_fit(ComputedStyle::ObjectFit::ScaleDown, big, 100.0f, 100.0f);
    EXPECT_FLOAT_EQ(up.dest.width, 100.0f);
    EXPECT_FLOAT_EQ(up.dest.height, 100.0f);
    EXPECT_FLOAT_EQ(up.dest.x, 50.0f);  // centered in the 200px box
}

// --- Story 8.5.3: overflow:hidden paint-time clip ---------------------------

namespace {
struct OverflowPaint {
    Hummingbird::Test::TestGraphicsContext context;
    Rect root_rect;
};

// Lays out a <div><div></div></div> in a 200-wide box and paints it, returning
// the recording context plus the root's laid-out border box. The caller sets the
// root's overflow (and optionally a uniform border) before calling.
OverflowPaint paint_overflow_fixture(ComputedStyle::Overflow ox, ComputedStyle::Overflow oy, float border = 0.0f) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, "div");
    root->append_child(DomFactory::create_element(arena, "div"));

    auto root_style = std::make_shared<ComputedStyle>(default_computed_style());
    root_style->overflow_x = ox;
    root_style->overflow_y = oy;
    root_style->border_width = {border, border, border, border};
    root->set_computed_style(root_style);
    root->get_children()[0]->set_computed_style(std::make_shared<ComputedStyle>(default_computed_style()));

    auto render_root = BlockBox::create(root.get());
    render_root->append_child(BlockBox::create(dynamic_cast<Element*>(root->get_children()[0].get())));

    OverflowPaint out;
    Rect bounds{0, 0, 200, 100};
    render_root->layout(out.context, bounds);
    out.root_rect = render_root->get_rect();
    render_root->paint(out.context, Point{0, 0});
    return out;
}
}  // namespace

TEST(OverflowClipTest, OverflowHiddenClipsChildrenToBorderBox) {
    auto out = paint_overflow_fixture(ComputedStyle::Overflow::Hidden, ComputedStyle::Overflow::Hidden);
    EXPECT_EQ(out.context.push_clip_count, 1);
    EXPECT_EQ(out.context.pop_clip_count, 1);
    ASSERT_EQ(out.context.pushed_clips.size(), 1u);
    // The clip is the root's border box (root painted at offset 0,0).
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].x, out.root_rect.x);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].y, out.root_rect.y);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].width, out.root_rect.width);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].height, out.root_rect.height);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].width, 200.0f);  // fills the container
}

TEST(OverflowClipTest, OverflowVisibleDoesNotClip) {
    auto out = paint_overflow_fixture(ComputedStyle::Overflow::Visible, ComputedStyle::Overflow::Visible);
    EXPECT_EQ(out.context.push_clip_count, 0);
    EXPECT_EQ(out.context.pop_clip_count, 0);
}

TEST(OverflowClipTest, SingleAxisHiddenDoesNotClip) {
    // Only overflow-x hidden: a rectangular clip cannot leave the y axis
    // unbounded, so by design we do not clip this case.
    auto out = paint_overflow_fixture(ComputedStyle::Overflow::Hidden, ComputedStyle::Overflow::Visible);
    EXPECT_EQ(out.context.push_clip_count, 0);
}

TEST(OverflowClipTest, ClipIsAtThePaddingBoxInsideTheBorder) {
    // With a real border the clip must sit INSIDE it (the padding box), so the
    // border stays visible under overflowing content. A zero-border fixture
    // cannot distinguish this from a border-box clip.
    auto out = paint_overflow_fixture(ComputedStyle::Overflow::Hidden, ComputedStyle::Overflow::Hidden,
                                      /*border=*/2.0f);
    ASSERT_EQ(out.context.pushed_clips.size(), 1u);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].x, out.root_rect.x + 2.0f);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].y, out.root_rect.y + 2.0f);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].width, out.root_rect.width - 4.0f);
    EXPECT_FLOAT_EQ(out.context.pushed_clips[0].height, out.root_rect.height - 4.0f);
}

namespace {
// Builds <div> with a left float (50x40) followed by a block, and returns the
// followed block's y after layout at width 200. The block's clear is set by
// the caller before layout.
struct ClearFixture {
    Hummingbird::Core::ArenaAllocator arena{2048};
    std::shared_ptr<Hummingbird::Css::ComputedStyle> block_style;
    Hummingbird::Core::ArenaPtr<Element> root;
    std::unique_ptr<BlockBox> render_root;

    ClearFixture() {
        root = DomFactory::create_element(arena, "div");
        root->append_child(DomFactory::create_element(arena, "div"));
        root->append_child(DomFactory::create_element(arena, "div"));
        root->set_computed_style(
            std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style()));

        auto float_style =
            std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
        float_style->float_type = Hummingbird::Css::ComputedStyle::Float::Left;
        float_style->width = Hummingbird::Css::ComputedStyle::LengthValue::from_px(50.0f);
        float_style->height = Hummingbird::Css::ComputedStyle::LengthValue::from_px(40.0f);
        root->get_children()[0]->set_computed_style(float_style);

        block_style = std::make_shared<Hummingbird::Css::ComputedStyle>(Hummingbird::Css::default_computed_style());
        block_style->height = Hummingbird::Css::ComputedStyle::LengthValue::from_px(10.0f);
        root->get_children()[1]->set_computed_style(block_style);
    }

    float run() {
        render_root = BlockBox::create(root.get());
        render_root->append_child(BlockBox::create(root->get_children()[0].get()));
        render_root->append_child(BlockBox::create(root->get_children()[1].get()));
        Hummingbird::Test::TestGraphicsContext context;
        Rect bounds{0, 0, 200, 0};
        render_root->layout(context, bounds);
        return render_root->get_children()[1]->get_rect().y;
    }
};
}  // namespace

TEST(BlockBoxLayoutTest, ClearLeftDropsBlockBelowLeftFloat) {
    ClearFixture fixture;
    fixture.block_style->clear = Hummingbird::Css::ComputedStyle::Clear::Left;
    EXPECT_GE(fixture.run(), 40.0f);
}

TEST(BlockBoxLayoutTest, ClearBothDropsBlockBelowLeftFloat) {
    ClearFixture fixture;
    fixture.block_style->clear = Hummingbird::Css::ComputedStyle::Clear::Both;
    EXPECT_GE(fixture.run(), 40.0f);
}

TEST(BlockBoxLayoutTest, ClearRightDoesNotClearLeftFloat) {
    // clear:right must ignore a left float, so the block stays beside it.
    ClearFixture fixture;
    fixture.block_style->clear = Hummingbird::Css::ComputedStyle::Clear::Right;
    EXPECT_LT(fixture.run(), 40.0f);
}

TEST(BlockBoxLayoutTest, NoClearLeavesBlockBesideFloat) {
    ClearFixture fixture;  // default clear = None
    EXPECT_LT(fixture.run(), 40.0f);
}
