#include "style/SelectorMatcher.h"

#include <gtest/gtest.h>

#include "core/dom/Element.h"
#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(SelectorMatcherTest, MatchesTagClassId) {
    Element elem(Hummingbird::Html::TagNames::Div);
    elem.set_attribute("class", "foo bar");
    elem.set_attribute("id", "main");

    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Tag, std::string(Hummingbird::Html::TagNames::Div)}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Tag, std::string(Hummingbird::Html::TagNames::Span)}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Class, "foo"}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Class, "bar"}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Class, "baz"}));
    EXPECT_TRUE(matches_selector(&elem, Selector{SelectorType::Id, "main"}));
    EXPECT_FALSE(matches_selector(&elem, Selector{SelectorType::Id, "other"}));
}
