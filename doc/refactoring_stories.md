1. Architecture & Modularity (The "Loose Coupling" Check)

    Story R.1: Dependency Injection Audit

        As a system architect,

        I want to verify that no "logic" modules (Layout, Style, DOM) have #include paths pointing to platform/ or app/.

        Acceptance:

            src/layout, src/style, and src/parser must only include headers from src/core or their own folder.

            Any platform-specific need (e.g., text width calculation) must go through an Interface (like IGraphicsContext).

        Why: This ensures you can swap SDL for GLFW or Blend2D for Skia without touching the core engine logic.

    Story R.2: The "Public Interface" Seal

        As a library developer,

        I want to hide internal implementation details of my modules using the Pimpl idiom (or just careful header separation) where compilation times are slowing down.

        Acceptance:

            Heavy implementation headers (like SDLGraphicsContext.h) are not included in public headers (like IWindow.h).

            Forward declarations are used in .h files wherever possible instead of #include.

    Story R.3: Namespace & Naming Hygiene

        As a developer,

        I want to disambiguate common names to prevent collisions.

        Context: You currently have parser/Parser.cpp (HTML?) and style/Parser.cpp (CSS).

        Acceptance:

            Ensure distinct namespaces: Hummingbird::Html::Parser vs Hummingbird::Css::Parser.

            Renaming files if necessary (e.g., HtmlParser.cpp, CssParser.cpp) for easier file searching in IDEs.

2. C++20 Modernization (The "Speed & Safety" Check)

    Story R.4: std::string_view Retrofit

        As a performance engineer,

        I want to replace const std::string& parameters with std::string_view in all Parser, Tokenizer, and Layout function signatures.

        Acceptance:

            Tokenizers must not allocate memory for substrings; they should point to the original source buffer.

            Zero new string allocations during the parsing phase (excluding the final DOM text node creation).

    Story R.5: Concept Constraints

        As a developer,

        I want to replace generic typename T templates with C++20 concept constraints.

        Acceptance:

            Example: If a generic function expects a Node, use template <IsNode T> instead of just T.

            This provides readable compiler errors instead of "template substitution failure" walls of text.

    Story R.6: std::span for Buffers

        As a safety engineer,

        I want to replace raw pointer + length pairs (char* data, size_t len) with std::span<char>.

        Acceptance:

            Networking buffers and pixel buffers utilize std::span to prevent buffer overflows.

3. Memory & Performance (The "Footprint" Check)

    Story R.7: Arena Allocator Enforcement

        As a memory optimizer,

        I want to ensure all short-lived objects (DOM Nodes, RenderObjects, CSS Rules) are allocated via the ArenaAllocator, not new or std::make_unique.

        Acceptance:

            delete is never called manually on a DOM node.

            Verify that Core::ArenaAllocator is being reset/cleared when a page reloads.

    Story R.8: Struct Packing & Alignment

        As a low-level optimizer,

        I want to review the member order of high-volume classes (Node, RenderObject, Token).

        Acceptance:

            Reorder class members to minimize padding. (e.g., put bools next to each other, not between pointers).

            Goal: Keep sizeof(Node) under 64 bytes (cache line size) if possible.

4. Testing & Stability

    Story R.9: Test Isolation Verification

        As a QA engineer,

        I want to ensure unit tests in TESTS/ do not require a running window or GPU context.

        Acceptance:

            Layout tests use a MockGraphicsContext (which you have partially as TestGraphicsContext.h) to verify "draw calls" without actually drawing pixels.

            Tests run in CI environment (headless).