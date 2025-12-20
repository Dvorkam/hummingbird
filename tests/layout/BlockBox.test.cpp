#include "layout/BlockBox.h"

#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "layout/TreeBuilder.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;

TEST(BlockBoxLayoutTest, SimpleStacking) {
    // Create a DOM tree: <body><p/><p/></body>
    ArenaAllocator arena(2048);
    auto dom_root = make_arena_ptr<Element>(arena, "body");
    auto p1 = make_arena_ptr<Element>(arena, "p");
    auto p2 = make_arena_ptr<Element>(arena, "p");
    // Give the paragraphs some fake height for testing layout
    // In a real scenario, this would come from child text nodes or CSS
    p1->set_attribute("height", "10");
    p2->set_attribute("height", "20");
    dom_root->append_child(std::move(p1));
    dom_root->append_child(std::move(p2));

    // Build the render tree
    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    // Create a dummy layout function for the test that gives a fixed height
    // This is a hack for now. A real implementation would calculate height from children.
    class TestBlockBox : public BlockBox {
    public:
        using BlockBox::BlockBox;
        void layout(IGraphicsContext& context, const Rect& bounds) override {
            BlockBox::layout(context, bounds);
            const auto* element_node = dynamic_cast<const Hummingbird::DOM::Element*>(get_dom_node());
            if (element_node) {
                const auto& attributes = element_node->get_attributes();
                auto it = attributes.find("height");
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
    TestGraphicsContext context;
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
