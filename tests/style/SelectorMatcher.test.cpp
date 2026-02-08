#include "style/selector/SelectorMatcher.h"

#include <gtest/gtest.h>

#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;
namespace Attr = Hummingbird::Html::AttributeNames;

namespace {
SelectorPart make_part(std::string_view tag = "", std::string_view id = "", std::vector<std::string> classes = {}) {
    SelectorPart part;
    part.tag = std::string(tag);
    part.id = std::string(id);
    part.classes = std::move(classes);
    return part;
}
}  // namespace

TEST(SelectorMatcherTest, MatchesTagClassId) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    elem->set_attribute(Attr::Class, "foo bar");
    elem->set_attribute(Attr::Id, "main");

    Selector tag_selector;
    tag_selector.parts.push_back(make_part(Hummingbird::Html::TagNames::Div));
    EXPECT_TRUE(matches_selector(elem.get(), tag_selector));

    Selector wrong_tag;
    wrong_tag.parts.push_back(make_part(Hummingbird::Html::TagNames::Span));
    EXPECT_FALSE(matches_selector(elem.get(), wrong_tag));

    Selector class_selector;
    class_selector.parts.push_back(make_part("", "", {"foo"}));
    EXPECT_TRUE(matches_selector(elem.get(), class_selector));

    Selector class_selector_two;
    class_selector_two.parts.push_back(make_part("", "", {"bar"}));
    EXPECT_TRUE(matches_selector(elem.get(), class_selector_two));

    Selector missing_class;
    missing_class.parts.push_back(make_part("", "", {"baz"}));
    EXPECT_FALSE(matches_selector(elem.get(), missing_class));

    Selector id_selector;
    id_selector.parts.push_back(make_part("", "main"));
    EXPECT_TRUE(matches_selector(elem.get(), id_selector));

    Selector wrong_id;
    wrong_id.parts.push_back(make_part("", "other"));
    EXPECT_FALSE(matches_selector(elem.get(), wrong_id));

    Selector compound;
    compound.parts.push_back(make_part(Hummingbird::Html::TagNames::Div, "main", {"foo", "bar"}));
    EXPECT_TRUE(matches_selector(elem.get(), compound));
}

TEST(SelectorMatcherTest, NormalizesAttributeKeys) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    elem->set_attribute("CLASS", "foo");
    elem->set_attribute("ID", "main");

    Selector selector;
    selector.parts.push_back(make_part("", "main", {"foo"}));
    EXPECT_TRUE(matches_selector(elem.get(), selector));
}

TEST(SelectorMatcherTest, MatchesUniversalSelector) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);

    Selector selector;
    selector.parts.push_back(make_part("*"));
    EXPECT_TRUE(matches_selector(elem.get(), selector));
}

TEST(SelectorMatcherTest, MatchesDescendantSelector) {
    Hummingbird::Core::ArenaAllocator arena(2048);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    child->set_attribute(Attr::Class, "note");
    root->append_child(std::move(child));

    Selector selector;
    selector.parts.push_back(make_part(Hummingbird::Html::TagNames::Div));
    selector.parts.push_back(make_part("", "", {"note"}));
    const auto* target = dynamic_cast<const Element*>(root->get_children()[0].get());
    ASSERT_NE(target, nullptr);
    EXPECT_TRUE(matches_selector(target, selector));

    Selector mismatch;
    mismatch.parts.push_back(make_part(Hummingbird::Html::TagNames::Span));
    mismatch.parts.push_back(make_part("", "", {"note"}));
    EXPECT_FALSE(matches_selector(target, mismatch));
}
