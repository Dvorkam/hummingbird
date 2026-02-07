#include "style/registry/CssPropertyRegistry.h"

#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "html/HtmlTagNames.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;

TEST(CssPropertyRegistryTest, MapsCanonicalAndAliasNames) {
    EXPECT_EQ(PropertyRegistry::parse_property_name("margin"), Property::Margin);
    EXPECT_EQ(PropertyRegistry::parse_property_name("-webkit-box-sizing"), Property::BoxSizing);
    EXPECT_EQ(PropertyRegistry::canonical_property_name(Property::BoxSizing), "box-sizing");
    EXPECT_TRUE(PropertyRegistry::canonical_property_name(Property::Unknown).empty());
}

TEST(CssPropertyRegistryTest, EntriesRoundTripAndHooksExist) {
    for (const auto& entry : PropertyRegistry::entries()) {
        EXPECT_EQ(PropertyRegistry::parse_property_name(entry.name), entry.property);
        EXPECT_FALSE(PropertyRegistry::canonical_property_name(entry.property).empty());
        EXPECT_NE(PropertyRegistry::parser_hook(entry.property), PropertyRegistry::ParserHook::Unknown);
        EXPECT_NE(PropertyRegistry::applier_hook(entry.property), PropertyRegistry::ApplyHook::Unknown);
    }
}

TEST(CssPropertyRegistryTest, ParserDispatchesMarginThroughHook) {
    ASSERT_EQ(PropertyRegistry::parser_hook(Property::Margin), PropertyRegistry::ParserHook::parse_margin_shorthand);

    Parser parser("div { margin: 1px 2px; }");
    auto sheet = parser.parse();
    ASSERT_EQ(sheet.rules.size(), 1u);
    const auto& decls = sheet.rules[0].declarations;
    ASSERT_EQ(decls.size(), 4u);
    EXPECT_EQ(decls[0].property, Property::MarginTop);
    EXPECT_EQ(decls[1].property, Property::MarginRight);
    EXPECT_EQ(decls[2].property, Property::MarginBottom);
    EXPECT_EQ(decls[3].property, Property::MarginLeft);
}

TEST(CssPropertyRegistryTest, ApplierDispatchesAliasThroughHook) {
    ASSERT_EQ(PropertyRegistry::parse_property_name("-webkit-box-sizing"), Property::BoxSizing);
    ASSERT_EQ(PropertyRegistry::applier_hook(Property::BoxSizing), PropertyRegistry::ApplyHook::apply_box_sizing);

    Hummingbird::Core::ArenaAllocator arena(1024);
    auto root = DomFactory::create_element(arena, Hummingbird::Html::TagNames::Div);

    Parser parser("div { -webkit-box-sizing: border-box; }");
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, root.get());

    auto style = root->get_computed_style();
    ASSERT_TRUE(style);
    EXPECT_EQ(style->box_sizing, ComputedStyle::BoxSizing::BorderBox);
}
