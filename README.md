# Hummingbird Browser Engine

Hummingbird is an experimental, lightweight browser engine being built from scratch in C++20. The goal of this project is educational, aiming to explore the inner workings of a browser, from HTML parsing and DOM creation to rendering and layout.

## Milestones

-   [x] **Epic 0: The Engine Foundation** - A working "Hello Engine" application that can open a window and render a solid color. The basic build system and modular architecture are in place.
-   [ ] **Epic 1: The HTML Parser**
-   [ ] **Epic 2: The CSS Parser**
-   [ ] **Epic 3: The DOM & Layout Tree**
-   [ ] **Epic 4: The Render Tree & Painting**

## Getting Started

### Prerequisites

-   A C++20 compatible compiler (e.g., MSVC, GCC, Clang).
-   [CMake](https://cmake.org/download/) (version 3.20 or later).
-   [vcpkg](https://github.com/microsoft/vcpkg) for dependency management. Ensure you have the `VCPKG_ROOT` environment variable set.

### Building

This project uses a `vcpkg.json` manifest to declare its dependencies. They will be installed automatically by CMake.

1.  **Clone the repository:**
    ```bash
    git clone <repository-url>
    cd hummingbird
    ```

2.  **Configure CMake:**
    Run the following command from the root of the project. This will configure the project and download the dependencies into a `vcpkg_installed` directory.
    ```bash
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
    ```
    *(On Linux/macOS, use `$VCPKG_ROOT` instead of `%VCPKG_ROOT%`)*

3.  **Build the project:**
    ```bash
    cmake --build build
    ```

4.  **Run the application:**
    The executable will be located in the `build/Debug` directory.
    ```bash
    ./build/Debug/Hummingbird
    ```
