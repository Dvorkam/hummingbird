#include "style/SelectorMatcher.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;
namespace Attr = Hummingbird::Html::AttributeNames;

TEST(SelectorMatcherTest, MatchesTagClassId) {
    ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    elem->set_attribute(Attr::Class, "foo bar");
    elem->set_attribute(Attr::Id, "main");

    Selector tag_selector;
    tag_selector.tag = Hummingbird::Html::TagNames::Div;
    EXPECT_TRUE(matches_selector(elem.get(), tag_selector));

    Selector wrong_tag;
    wrong_tag.tag = Hummingbird::Html::TagNames::Span;
    EXPECT_FALSE(matches_selector(elem.get(), wrong_tag));

    Selector class_selector;
    class_selector.classes = {"foo"};
    EXPECT_TRUE(matches_selector(elem.get(), class_selector));

    Selector class_selector_two;
    class_selector_two.classes = {"bar"};
    EXPECT_TRUE(matches_selector(elem.get(), class_selector_two));

    Selector missing_class;
    missing_class.classes = {"baz"};
    EXPECT_FALSE(matches_selector(elem.get(), missing_class));

    Selector id_selector;
    id_selector.id = "main";
    EXPECT_TRUE(matches_selector(elem.get(), id_selector));

    Selector wrong_id;
    wrong_id.id = "other";
    EXPECT_FALSE(matches_selector(elem.get(), wrong_id));

    Selector compound;
    compound.tag = Hummingbird::Html::TagNames::Div;
    compound.id = "main";
    compound.classes = {"foo", "bar"};
    EXPECT_TRUE(matches_selector(elem.get(), compound));
}

TEST(SelectorMatcherTest, NormalizesAttributeKeys) {
    ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    elem->set_attribute("CLASS", "foo");
    elem->set_attribute("ID", "main");

    Selector selector;
    selector.classes = {"foo"};
    selector.id = "main";
    EXPECT_TRUE(matches_selector(elem.get(), selector));
}
