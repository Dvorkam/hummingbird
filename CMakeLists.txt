cmake_minimum_required(VERSION 3.20)
project(Hummingbird)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



find_package(SDL2 REQUIRED)
find_package(blend2d REQUIRED)

# --- Core Library ---
# This library will contain the abstract interfaces and platform-independent logic.
add_library(Core STATIC
    src/core/ArenaAllocator.cpp
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
)
target_include_directories(Platform
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Platform PUBLIC Core SDL2::SDL2 blend2d::blend2d)

# --- Parser Library ---
add_library(Parser STATIC
    src/parser/Tokenizer.cpp
    src/parser/Parser.cpp
)
target_include_directories(Parser
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
target_link_libraries(Hummingbird PRIVATE Platform Parser)

# --- Unit Testing ---
enable_testing()
add_subdirectory(tests)