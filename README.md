# Hummingbird Browser Engine

Hummingbird is an experimental, lightweight browser engine being built from scratch in C++20. The goal of this project is educational, aiming to explore the inner workings of a browser, from HTML parsing and DOM creation to rendering and layout.

## Milestones

-   [x] **Epic 0: The Engine Foundation** — windowing, graphics context, arena allocator, and stubbed networking in place.
-   [!] **Epic 1: HTML → DOM → Render (in progress)** — tokenizer, DOM builder, UA defaults for lists/links/headings/code, block + inline layout, render tree + painter. Next up: render-tree construction refinements and inline text flow improvements (Epic 1.4+).
-   [ ] **Epic 2: CSS Parser / Cascade** — real stylesheet inputs and broader selector support.
-   [ ] **Epic 3: DOM & Layout Tree** — richer layout behaviors and block/inline refinements.
-   [ ] **Epic 4: Render Tree & Painting** — full paint traversal and debugging overlays.

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

## Architecture

The project follows Ports & Adapters. Core logic stays decoupled from platform implementations.

-   `src/app`: application wiring (event loop, pipeline orchestration).
-   `src/core`: foundational types (arena allocator, asset paths) and interfaces (`IWindow`, `IGraphicsContext`, `INetwork`).
-   `src/html`: HTML tokenizer and DOM builder.
-   `src/style`: CSS tokenizer/parser, selector matching, and computed style production.
-   `src/layout`: render objects, tree builder, block + inline layout.
-   `src/renderer`: painter that walks the render tree.
-   `src/platform`: SDL window/graphics and Curl networking adapters.
