#include "style/selector/SelectorMatcher.h"

#include <gtest/gtest.h>

#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
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

TEST(SelectorMatcherTest, MatchesChildSelector) {
    Hummingbird::Core::ArenaAllocator arena(3072);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto parent = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Section);
    auto child = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    child->set_attribute(Attr::Class, "note");
    parent->append_child(std::move(child));
    root->append_child(std::move(parent));

    const auto* target = dynamic_cast<const Element*>(root->get_children()[0]->get_children()[0].get());
    ASSERT_NE(target, nullptr);

    Selector child_selector;
    child_selector.parts.push_back(make_part(Hummingbird::Html::TagNames::Section));
    child_selector.parts.push_back(make_part("", "", {"note"}));
    child_selector.combinators.push_back(Selector::Combinator::Child);
    EXPECT_TRUE(matches_selector(target, child_selector));

    Selector wrong_child_selector;
    wrong_child_selector.parts.push_back(make_part(Hummingbird::Html::TagNames::Div));
    wrong_child_selector.parts.push_back(make_part("", "", {"note"}));
    wrong_child_selector.combinators.push_back(Selector::Combinator::Child);
    EXPECT_FALSE(matches_selector(target, wrong_child_selector));

    Selector descendant_selector;
    descendant_selector.parts.push_back(make_part(Hummingbird::Html::TagNames::Div));
    descendant_selector.parts.push_back(make_part("", "", {"note"}));
    descendant_selector.combinators.push_back(Selector::Combinator::Descendant);
    EXPECT_TRUE(matches_selector(target, descendant_selector));
}

TEST(SelectorMatcherTest, MatchesAdjacentSiblingSelector) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto parent = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto first = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    first->set_attribute(Attr::Class, "first");
    auto second = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    second->set_attribute(Attr::Class, "second");
    auto third = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    third->set_attribute(Attr::Class, "third");
    parent->append_child(std::move(first));
    // Whitespace text between siblings must not break adjacency.
    parent->append_child(DomFactory::create_text(arena, " "));
    parent->append_child(std::move(second));
    parent->append_child(std::move(third));

    const auto* second_elem = dynamic_cast<const Element*>(parent->get_children()[2].get());
    const auto* third_elem = dynamic_cast<const Element*>(parent->get_children()[3].get());
    ASSERT_NE(second_elem, nullptr);
    ASSERT_NE(third_elem, nullptr);

    Selector adjacent;
    adjacent.parts.push_back(make_part("", "", {"first"}));
    adjacent.parts.push_back(make_part("", "", {"second"}));
    adjacent.combinators.push_back(Selector::Combinator::NextSibling);
    EXPECT_TRUE(matches_selector(second_elem, adjacent));

    // .first + .third does not match: .second sits between them.
    Selector not_adjacent;
    not_adjacent.parts.push_back(make_part("", "", {"first"}));
    not_adjacent.parts.push_back(make_part("", "", {"third"}));
    not_adjacent.combinators.push_back(Selector::Combinator::NextSibling);
    EXPECT_FALSE(matches_selector(third_elem, not_adjacent));

    // No previous sibling at all.
    const auto* first_elem = dynamic_cast<const Element*>(parent->get_children()[0].get());
    ASSERT_NE(first_elem, nullptr);
    Selector no_previous;
    no_previous.parts.push_back(make_part("", "", {"second"}));
    no_previous.parts.push_back(make_part("", "", {"first"}));
    no_previous.combinators.push_back(Selector::Combinator::NextSibling);
    EXPECT_FALSE(matches_selector(first_elem, no_previous));
}

TEST(SelectorMatcherTest, MatchesGeneralSiblingSelector) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto parent = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);
    auto input = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    input->set_attribute(Attr::Class, "search__input");
    auto spacer = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Span);
    auto button = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Button);
    button->set_attribute(Attr::Class, "search__button");
    parent->append_child(std::move(input));
    parent->append_child(std::move(spacer));
    parent->append_child(std::move(button));

    auto* input_elem = dynamic_cast<Element*>(parent->get_children()[0].get());
    const auto* button_elem = dynamic_cast<const Element*>(parent->get_children()[2].get());
    ASSERT_NE(input_elem, nullptr);
    ASSERT_NE(button_elem, nullptr);

    // DDG: .search__input:focus ~ .search__button
    Selector focus_sibling;
    SelectorPart input_part = make_part("", "", {"search__input"});
    input_part.pseudo_classes.push_back(SelectorPart::PseudoClass::Focus);
    focus_sibling.parts.push_back(std::move(input_part));
    focus_sibling.parts.push_back(make_part("", "", {"search__button"}));
    focus_sibling.combinators.push_back(Selector::Combinator::SubsequentSibling);

    EXPECT_FALSE(matches_selector(button_elem, focus_sibling));
    input_elem->set_pseudo_state(Element::PseudoState::Focus, true);
    EXPECT_TRUE(matches_selector(button_elem, focus_sibling));

    // The combinator only looks backwards: the input has no preceding button.
    Selector backwards;
    backwards.parts.push_back(make_part("", "", {"search__button"}));
    backwards.parts.push_back(make_part("", "", {"search__input"}));
    backwards.combinators.push_back(Selector::Combinator::SubsequentSibling);
    EXPECT_FALSE(matches_selector(input_elem, backwards));
}

TEST(SelectorMatcherTest, MatchesPseudoClassStates) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto elem = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Input);
    elem->set_pseudo_state(Element::PseudoState::Focus, true);

    Selector focus_selector;
    SelectorPart focus_part = make_part(Hummingbird::Html::TagNames::Input);
    focus_part.pseudo_classes.push_back(SelectorPart::PseudoClass::Focus);
    focus_selector.parts.push_back(std::move(focus_part));
    EXPECT_TRUE(matches_selector(elem.get(), focus_selector));

    Selector hover_selector;
    SelectorPart hover_part = make_part(Hummingbird::Html::TagNames::Input);
    hover_part.pseudo_classes.push_back(SelectorPart::PseudoClass::Hover);
    hover_selector.parts.push_back(std::move(hover_part));
    EXPECT_FALSE(matches_selector(elem.get(), hover_selector));
}
