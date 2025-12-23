# Find the GTest package
find_package(GTest REQUIRED)

# Create the test executable
add_executable(HummingbirdTests
    core/ArenaAllocator.test.cpp
    core/AssetPath.test.cpp
    html/HtmlTokenizer.test.cpp
    html/HtmlParser.test.cpp
    layout/TreeBuilder.test.cpp
    layout/BlockBox.test.cpp
    layout/TextBox.test.cpp
    layout/InlineLayout.test.cpp
    layout/InlineLineBuilder.test.cpp
    layout/ListItemLayout.test.cpp
    renderer/Painter.test.cpp
    style/CSSParser.test.cpp
    style/SelectorMatcher.test.cpp
    style/StyleEngine.test.cpp
    layout/LayoutStyleIntegration.test.cpp
    network/StubNetwork.test.cpp
    network/CurlNetwork.test.cpp
)

# Link the test executable against GTest and our libraries
target_link_libraries(HummingbirdTests PRIVATE
    GTest::gtest_main
    Core
    Html
    Layout
    Renderer
    Style
    Platform
)
target_compile_definitions(HummingbirdTests PRIVATE HB_ENABLE_LOG)

# Add the root src directory to the include path so tests can find headers
target_include_directories(HummingbirdTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Add the test executable to the list of tests
include(GoogleTest)
gtest_discover_tests(HummingbirdTests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
