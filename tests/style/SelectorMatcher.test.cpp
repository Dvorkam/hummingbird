#include "style/SelectorMatcher.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(SelectorMatcherTest, MatchesTagClassId) {
    ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    elem->set_attribute("class", "foo bar");
    elem->set_attribute("id", "main");

    EXPECT_TRUE(matches_selector(elem.get(), Selector{SelectorType::Tag, Hummingbird::Html::TagNames::Div}));
    EXPECT_FALSE(matches_selector(elem.get(), Selector{SelectorType::Tag, Hummingbird::Html::TagNames::Span}));
    EXPECT_TRUE(matches_selector(elem.get(), Selector{SelectorType::Class, "foo"}));
    EXPECT_TRUE(matches_selector(elem.get(), Selector{SelectorType::Class, "bar"}));
    EXPECT_FALSE(matches_selector(elem.get(), Selector{SelectorType::Class, "baz"}));
    EXPECT_TRUE(matches_selector(elem.get(), Selector{SelectorType::Id, "main"}));
    EXPECT_FALSE(matches_selector(elem.get(), Selector{SelectorType::Id, "other"}));
}
