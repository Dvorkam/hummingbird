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
    engine/TabManager.test.cpp
    engine/ExtensionManifest.test.cpp
    engine/ExtensionSettings.test.cpp
    engine/ExtensionLoader.test.cpp
    engine/ExtensionHost.test.cpp
    engine/ResourceStore.test.cpp
    engine/RedirectPolicy.test.cpp
    engine/RedirectChain.test.cpp
    engine/DocumentCookie.test.cpp
    engine/RequestDeadline.test.cpp
    engine/LocalStorage.test.cpp
    engine/FontFaceResolver.test.cpp
    core/ArenaAllocator.test.cpp
    core/BookmarkStore.test.cpp
    core/HttpHeaders.test.cpp
    core/CookieJar.test.cpp
    core/PublicSuffix.test.cpp
    core/BrowserIdentity.test.cpp
    core/IdentityPolicyStore.test.cpp
    core/Origin.test.cpp
    core/Referrer.test.cpp
    core/StorageArea.test.cpp
    core/StorageManager.test.cpp
    core/AssetPath.test.cpp
    core/AssetLoader.test.cpp
    core/TextEditBuffer.test.cpp
    core/Url.test.cpp
    core/UrlEncoding.test.cpp
    core/Timing.test.cpp
    core/WarnOnce.test.cpp
    core/CompatibilityWarnings.test.cpp
    core/ElementSemanticRole.test.cpp
    core/DependencyFirewall.test.cpp
    html/HtmlTokenizer.test.cpp
    html/HtmlParser.test.cpp
    layout/TreeBuilder.test.cpp
    layout/BlockBox.test.cpp
    layout/TextBox.test.cpp
    layout/InlineLayout.test.cpp
    layout/InlineLineBuilder.test.cpp
    layout/ListItemLayout.test.cpp
    layout/TableLayout.test.cpp
    layout/TableColumnLayout.test.cpp
    layout/Positioning.test.cpp
    layout/FlexLayout.test.cpp
    layout/GridLayout.test.cpp
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
    engine/ScriptController.test.cpp
    engine/TodoMvc.test.cpp
    engine/LoginFlow.test.cpp
    engine/SessionStorage.test.cpp
    platform/ResourceProvider.test.cpp
    platform/ImageDecoder.test.cpp
    platform/ScriptEngine.test.cpp
    platform/CacheUtils.test.cpp
    platform/Blend2DFontCache.test.cpp
    network/StubNetwork.test.cpp
    network/NetworkSeamHeaders.test.cpp
    network/CurlNetwork.test.cpp
    network/NetworkFactory.test.cpp
    layout/DdgHomeLayout.test.cpp
    fuzz/FuzzTargets.cpp
    fuzz/ParserFuzzSmoke.test.cpp
)

target_compile_definitions(HummingbirdTests PRIVATE
    HB_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    HB_FUZZ_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus"
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
    app/BrowserAppTabs.test.cpp
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

# --- libFuzzer parser targets (story 7.5.3) --------------------------------
# HB_ENABLE_FUZZING is defined at the top level, where it also instruments the
# whole build (coverage + ASan). The cross-platform smoke test above exercises
# the same harness + corpus in every build; these executables are for real
# coverage-guided fuzzing (the CI `fuzz` job runs them briefly, longer runs are
# local). They only need to link the libFuzzer driver on top of the global flags.
if(HB_ENABLE_FUZZING)
    foreach(fuzz_name html_parser css_parser)
        add_executable(${fuzz_name}_fuzzer
            fuzz/FuzzTargets.cpp
            fuzz/${fuzz_name}_fuzzer.cpp
        )
        target_include_directories(${fuzz_name}_fuzzer PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}
        )
        target_link_libraries(${fuzz_name}_fuzzer PRIVATE Core Html Style)
        target_link_options(${fuzz_name}_fuzzer PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
