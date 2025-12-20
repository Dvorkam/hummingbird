#include <gtest/gtest.h>
#include "style/SelectorMatcher.h"
#include "core/dom/Element.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(SelectorMatcherTest, MatchesTagClassId) {
    Element elem("div");
    elem.set_attribute("class", "foo bar");
    elem.set_attribute("id", "main");

    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Tag, "div"}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Tag, "span"}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Class, "foo"}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Class, "bar"}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Class, "baz"}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Id, "main"}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Id, "other"}));
}
