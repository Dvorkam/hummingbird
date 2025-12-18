# Find the GTest package
find_package(GTest REQUIRED)

# Create the test executable
add_executable(HummingbirdTests
    core/ArenaAllocator.test.cpp
    parser/Tokenizer.test.cpp
    parser/Parser.test.cpp
)

# Link the test executable against GTest and our libraries
target_link_libraries(HummingbirdTests PRIVATE
    GTest::gtest_main
    Core
    Parser
)

# Add the root src directory to the include path so tests can find headers
target_include_directories(HummingbirdTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Add the test executable to the list of tests
include(GoogleTest)
gtest_discover_tests(HummingbirdTests)
