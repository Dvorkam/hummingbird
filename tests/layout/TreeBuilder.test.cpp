#include "layout/TreeBuilder.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderBreak.h"
#include "layout/RenderRule.h"
#include "layout/TextBox.h"
#include "style/CssParser.h"
#include "style/StyleEngine.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;
namespace TagNames = Hummingbird::Html::TagNames;

TEST(TreeBuilderTest, SimpleTree) {
    ArenaAllocator arena(1024);
    auto dom_root = DomFactory::create_element(arena, TagNames::Html);
    dom_root->append_child(DomFactory::create_element(arena, TagNames::Body));

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    EXPECT_EQ(render_root->get_dom_node(), dom_root.get());
    ASSERT_EQ(render_root->get_children().size(), 1);

    const auto* body_render_object = render_root->get_children()[0].get();
    EXPECT_EQ(body_render_object->get_dom_node(), dom_root->get_children()[0].get());
    EXPECT_EQ(body_render_object->get_children().size(), 0);
}

TEST(TreeBuilderTest, CreatesTextBoxForTextNode) {
    ArenaAllocator arena(1024);
    auto dom_root = DomFactory::create_element(arena, TagNames::P);
    dom_root->append_child(DomFactory::create_text(arena, "Hello"));

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1);

    const auto* text_box = dynamic_cast<const TextBox*>(render_root->get_children()[0].get());
    ASSERT_NE(text_box, nullptr);
}

TEST(TreeBuilderTest, CreatesBreakAndRuleForControlTags) {
    ArenaAllocator arena(1024);
    auto dom_root = DomFactory::create_element(arena, TagNames::Body);
    dom_root->append_child(DomFactory::create_element(arena, TagNames::Br));
    dom_root->append_child(DomFactory::create_element(arena, TagNames::Hr));

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 2u);
    EXPECT_NE(dynamic_cast<const RenderBreak*>(render_root->get_children()[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<const RenderRule*>(render_root->get_children()[1].get()), nullptr);
}

TEST(TreeBuilderTest, SkipsNonVisualNodesButKeepsRootContainer) {
    ArenaAllocator arena(2048);
    auto dom_root = DomFactory::create_element(arena, TagNames::Html);
    auto head = DomFactory::create_element(arena, TagNames::Head);
    head->append_child(DomFactory::create_element(arena, TagNames::Style));
    auto body = DomFactory::create_element(arena, TagNames::Body);
    body->append_child(DomFactory::create_element(arena, TagNames::Div));
    dom_root->append_child(std::move(head));
    dom_root->append_child(std::move(body));

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);  // head/style filtered out
    const auto* body_render = render_root->get_children()[0].get();
    ASSERT_NE(body_render, nullptr);
    EXPECT_EQ(body_render->get_children().size(), 1u);  // contains div
}

TEST(TreeBuilderTest, SkipsDisplayNoneElements) {
    ArenaAllocator arena(2048);
    auto dom_root = DomFactory::create_element(arena, TagNames::Body);
    auto visible = DomFactory::create_element(arena, TagNames::Div);
    auto hidden = DomFactory::create_element(arena, TagNames::Div);
    hidden->set_attribute("class", "hidden");
    dom_root->append_child(std::move(visible));
    dom_root->append_child(std::move(hidden));

    std::string css = ".hidden { display: none; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);
    EXPECT_EQ(render_root->get_children()[0]->get_dom_node(), dom_root->get_children()[0].get());
}

TEST(TreeBuilderTest, SkipsWhitespaceOnlyTextInNormalMode) {
    ArenaAllocator arena(1024);
    auto dom_root = DomFactory::create_element(arena, TagNames::P);
    dom_root->append_child(DomFactory::create_text(arena, " \n\t "));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    EXPECT_TRUE(render_root->get_children().empty());
}

TEST(TreeBuilderTest, PreservesWhitespaceOnlyTextInPreMode) {
    ArenaAllocator arena(1024);
    auto dom_root = DomFactory::create_element(arena, TagNames::Pre);
    dom_root->append_child(DomFactory::create_text(arena, " \n\t "));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder tree_builder;
    auto render_root = tree_builder.build(dom_root.get());

    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);
    const auto* text_box = dynamic_cast<const TextBox*>(render_root->get_children()[0].get());
    ASSERT_NE(text_box, nullptr);
}
