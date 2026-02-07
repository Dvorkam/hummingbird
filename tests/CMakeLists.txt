# Find the GTest package
find_package(GTest REQUIRED)

# Shared include paths for test targets.
add_library(HummingbirdTestSupport INTERFACE)
target_include_directories(HummingbirdTestSupport INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create the core test executable (no App dependency).
add_executable(HummingbirdTests
    test_utils/HeadlessTabHarness.h
    test_utils/TestFakes.h
    test_utils/TestGraphicsContext.h
    engine/Tab.test.cpp
    engine/ResourceStore.test.cpp
    core/ArenaAllocator.test.cpp
    core/AssetPath.test.cpp
    core/AssetLoader.test.cpp
    core/TextEditBuffer.test.cpp
    core/Url.test.cpp
    core/Timing.test.cpp
    core/WarnOnce.test.cpp
    html/HtmlTokenizer.test.cpp
    html/HtmlParser.test.cpp
    layout/TreeBuilder.test.cpp
    layout/BlockBox.test.cpp
    layout/TextBox.test.cpp
    layout/InlineLayout.test.cpp
    layout/InlineLineBuilder.test.cpp
    layout/ListItemLayout.test.cpp
    layout/TableLayout.test.cpp
    layout/Positioning.test.cpp
    layout/TextStyleUtils.test.cpp
    renderer/DisplayList.test.cpp
    renderer/Painter.test.cpp
    style/CSSParser.test.cpp
    style/CssPropertyRegistry.test.cpp
    style/SelectorMatcher.test.cpp
    style/StyleEngine.test.cpp
    style/StylesheetSource.test.cpp
    layout/LayoutStyleIntegration.test.cpp
    engine/DocumentPipeline.test.cpp
    engine/DocumentInputController.test.cpp
    engine/DocumentPainter.test.cpp
    engine/ResourceLoader.test.cpp
    engine/ScriptHost.test.cpp
    platform/ResourceProvider.test.cpp
    platform/ImageDecoder.test.cpp
    platform/ScriptEngine.test.cpp
    platform/CacheUtils.test.cpp
    platform/Blend2DFontCache.test.cpp
    network/StubNetwork.test.cpp
    network/CurlNetwork.test.cpp
    network/NetworkFactory.test.cpp
)

# Link the test executable against GTest and our libraries
target_link_libraries(HummingbirdTests PRIVATE
    GTest::gtest_main
    HummingbirdTestSupport
    Core
    Html
    Layout
    Renderer
    Style
    Engine
    Platform
)

# App-only smoke test target.
add_executable(HummingbirdAppTests
    app/SmokeMain.test.cpp
    app/UrlBar.test.cpp
)
target_link_libraries(HummingbirdAppTests PRIVATE
    GTest::gtest_main
    HummingbirdTestSupport
    App
)

# Add the test executable to the list of tests
include(GoogleTest)
gtest_discover_tests(HummingbirdTests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
gtest_discover_tests(HummingbirdAppTests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
