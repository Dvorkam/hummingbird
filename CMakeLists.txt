cmake_minimum_required(VERSION 3.20)
project(Hummingbird)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



find_package(SDL2 REQUIRED)
find_package(blend2d REQUIRED)
find_package(CURL REQUIRED)

# --- Style Library ---
add_library(Style STATIC
    src/style/Tokenizer.cpp
    src/style/Parser.cpp
    src/style/SelectorMatcher.cpp
    src/style/StyleEngine.cpp
)
target_include_directories(Style
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Core Library ---
# This library will contain the abstract interfaces and platform-independent logic.
add_library(Core STATIC
    src/core/ArenaAllocator.cpp
    src/core/AssetPath.cpp
)
target_include_directories(Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Platform Library ---
# This library will contain the platform-specific implementations (e.g., SDL2, Blend2D).
add_library(Platform STATIC
    src/platform/SDLWindow.cpp
    src/platform/SDLGraphicsContext.cpp
    src/platform/CurlNetwork.cpp
    src/platform/StubNetwork.cpp
)
target_include_directories(Platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Platform PUBLIC Core SDL2::SDL2 blend2d::blend2d CURL::libcurl)

# --- HTML Parser Library ---
add_library(Html STATIC
    src/html/HtmlTokenizer.cpp
    src/html/HtmlParser.cpp
)
target_include_directories(Html
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- Layout Library ---
add_library(Layout STATIC
    src/layout/RenderObject.cpp
    src/layout/TreeBuilder.cpp
    src/layout/BlockBox.cpp
    src/layout/TextBox.cpp
    src/layout/RenderBreak.cpp
    src/layout/RenderRule.cpp
)
target_include_directories(Layout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Layout PUBLIC blend2d::blend2d Style)

# --- Renderer Library ---
add_library(Renderer STATIC
    src/renderer/Painter.cpp
)
target_include_directories(Renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# --- App Executable ---
# This is the main application that brings everything together.
add_executable(Hummingbird src/app/main.cpp)

# The App needs to know about the Core interfaces and the Platform implementation.
target_include_directories(Hummingbird PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Hummingbird PRIVATE Platform Html Layout Renderer Style SDL2::SDL2main)
target_compile_definitions(Hummingbird PRIVATE HB_ENABLE_LOG)

# --- Unit Testing ---
enable_testing()
add_subdirectory(tests)
