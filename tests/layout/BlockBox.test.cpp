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
#include "layout/replaced/RenderImage.h"
#include "style/types/ComputedStyle.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
namespace Attr = Hummingbird::Html::AttributeNames;
using Hummingbird::IGraphicsContext;

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
    float_style->width = 50.0f;
    float_style->height = 10.0f;
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
