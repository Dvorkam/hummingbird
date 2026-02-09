#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "html/HtmlTagNames.h"

namespace {

using Hummingbird::DOM::Element;

TEST(ElementSemanticRoleTest, ReturnsImpliedLandmarkRolesForSemanticTags) {
    Hummingbird::Core::ArenaAllocator arena(1024);

    auto header = Element::create(arena, Hummingbird::Html::TagNames::Header);
    auto nav = Element::create(arena, Hummingbird::Html::TagNames::Nav);
    auto main = Element::create(arena, Hummingbird::Html::TagNames::Main);
    auto section = Element::create(arena, Hummingbird::Html::TagNames::Section);
    auto article = Element::create(arena, Hummingbird::Html::TagNames::Article);
    auto aside = Element::create(arena, Hummingbird::Html::TagNames::Aside);
    auto footer = Element::create(arena, Hummingbird::Html::TagNames::Footer);

    ASSERT_TRUE(header->get_accessibility_role().has_value());
    ASSERT_TRUE(nav->get_accessibility_role().has_value());
    ASSERT_TRUE(main->get_accessibility_role().has_value());
    ASSERT_TRUE(section->get_accessibility_role().has_value());
    ASSERT_TRUE(article->get_accessibility_role().has_value());
    ASSERT_TRUE(aside->get_accessibility_role().has_value());
    ASSERT_TRUE(footer->get_accessibility_role().has_value());

    EXPECT_EQ(header->get_accessibility_role().value(), "banner");
    EXPECT_EQ(nav->get_accessibility_role().value(), "navigation");
    EXPECT_EQ(main->get_accessibility_role().value(), "main");
    EXPECT_EQ(section->get_accessibility_role().value(), "region");
    EXPECT_EQ(article->get_accessibility_role().value(), "article");
    EXPECT_EQ(aside->get_accessibility_role().value(), "complementary");
    EXPECT_EQ(footer->get_accessibility_role().value(), "contentinfo");
}

TEST(ElementSemanticRoleTest, ExplicitRoleAttributeOverridesImpliedRole) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto header = Element::create(arena, Hummingbird::Html::TagNames::Header);
    header->set_attribute("role", "none");

    ASSERT_TRUE(header->get_accessibility_role().has_value());
    EXPECT_EQ(header->get_accessibility_role().value(), "none");
}

TEST(ElementSemanticRoleTest, ReturnsNoRoleForNonSemanticTagWithoutAttribute) {
    Hummingbird::Core::ArenaAllocator arena(1024);
    auto div = Element::create(arena, Hummingbird::Html::TagNames::Div);

    EXPECT_FALSE(div->get_accessibility_role().has_value());
}

}  // namespace
