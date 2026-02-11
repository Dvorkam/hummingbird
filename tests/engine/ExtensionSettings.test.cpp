#include "engine/extensions/ExtensionSettings.h"

#include <gtest/gtest.h>

TEST(ExtensionSettingsTest, ParsesExtensionStatesFromIniSection) {
    constexpr std::string_view kIni = R"INI(
      [extensions]
      dark-mode = disabled
      reader = enabled
    )INI";

    auto settings = Hummingbird::Engine::extension_settings_from_ini(kIni);
    EXPECT_TRUE(settings.disabled_ids.count("dark-mode") > 0);
    EXPECT_TRUE(settings.disabled_ids.count("reader") == 0);
}

TEST(ExtensionSettingsTest, IgnoresUnknownSectionsAndInvalidValues) {
    constexpr std::string_view kIni = R"INI(
      [ui]
      theme = dark

      [extensions]
      dark-mode = maybe
      adblock = off
    )INI";

    auto settings = Hummingbird::Engine::extension_settings_from_ini(kIni);
    EXPECT_TRUE(settings.disabled_ids.count("dark-mode") == 0);
    EXPECT_TRUE(settings.disabled_ids.count("adblock") > 0);
}

TEST(ExtensionSettingsTest, EnvOverridesIniDisabledState) {
    Hummingbird::Engine::ExtensionSettings from_ini;
    from_ini.disabled_ids.insert("dark-mode");

    Hummingbird::Engine::ExtensionSettings from_env;
    from_env.enabled_ids.insert("dark-mode");

    auto merged = Hummingbird::Engine::merge_extension_settings(std::move(from_ini), from_env);
    EXPECT_TRUE(merged.enabled_ids.count("dark-mode") > 0);
    EXPECT_TRUE(merged.disabled_ids.count("dark-mode") == 0);
}
