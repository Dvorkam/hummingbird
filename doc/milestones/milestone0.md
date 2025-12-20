Epic 0: The Engine Foundation (Milestone 0)

Goal: Establish the build system, memory strategy, and the abstract graphics layer. Constraint: No direct SDL2/Blend2D calls in the Core logic.

    Story 0.1: Build System & Dependency Management

        As a dev, I want a CMake/Meson setup that isolates Core, Platform, and App into separate libraries.

        Acceptance: Project compiles; Core cannot include headers from Platform.

    Story 0.2: The Memory Arena

        As a performance engineer, I want a linear ArenaAllocator so that I can batch-allocate DOM nodes and free them all at once when a tab closes.

        Acceptance: A simple allocator that provides memory blocks and tracks total usage.

    Story 0.3: The Window Interface

        As a core engine, I need an IWindow interface (abstract base class or Concept) to request a drawing surface without knowing about the OS.

        Acceptance: IWindow::open(), IWindow::update(), IWindow::close().

    Story 0.4: The Graphics Context Interface

        As a core engine, I need an IGraphicsContext interface to draw primitives (Rects, Text).

        Acceptance: drawRect(), clear(), present().

    Story 0.5: The SDL2 & Blend2D Implementation

        As a platform maintainer, I want to implement IWindow using SDL2 and IGraphicsContext using Blend2D.

        Acceptance: A generic "Hello Engine" executable that opens a window and paints a teal background.

Epic 1: The DOM Builder (Milestone 1 - Part A)

Goal: Parse raw HTML text into a tree structure in memory.

    Story 1.1: The String View

        As a memory optimizer, I want to use std::string_view (or a custom equivalent) for HTML tokens to avoid copying strings during parsing.

        Acceptance: Zero-copy substring operations.

    Story 1.2: The Tokenizer

        As a parser, I need to read a character stream and output tokens (StartTag, EndTag, CharacterData).

        Acceptance: Input <div>hi</div> -> Output [StartTag:div], [Char:hi], [EndTag:div].

    Story 1.3: The DOM Tree

        As a developer, I need Node, Element, and Text classes that can form a tree hierarchy.

        Acceptance: Nodes can have children and a parent pointer.

    Story 1.4: The Tree Constructor

        As a parser, I want to consume tokens and build the DOM tree, handling simple nesting.

        Acceptance: Parsing <html><body><p>Test</p></body></html> results in a correct tree structure in the debugger.

Epic 2: The Layout Engine (Milestone 1 - Part B)

Goal: Turn the DOM (data) into a Render Tree (geometry). This is the most complex part of a browser.

    Story 2.1: The Render Tree

        As a layout engine, I need a RenderObject tree separate from the DOM, where each object has x, y, width, height.

        Acceptance: One-to-one mapping (initially) between DOM Elements and RenderObjects.

    Story 2.2: The Block Box Model

        As a layout engine, I need to calculate geometry for block-level elements (<div>, <p>, <h1>) so they stack vertically.

        Acceptance: Elements perform width calculation (parent width) and height calculation (sum of children).

    Story 2.3: Basic Text Layout (Inline)

        As a layout engine, I need to calculate the width of text strings using the graphics backend.

        Acceptance: Text renders inside a box; the box expands to fit the text.

    Story 2.4: The Paint Traverser

        As a renderer, I want to walk the Render Tree and issue drawing commands to the IGraphicsContext.

        Acceptance: HTML text actually appears on the screen in the correct vertical order.

Epic 3: The Stylist (Milestone 2)

Goal: Separate formatting from content using a CSS subset.

    Story 3.1: CSS Tokenizer & Parser

        As a parser, I want to parse a CSS string into a Stylesheet object containing rules and declarations.

        Acceptance: Parse div { color: red; }.

    Story 3.2: The Selector Matcher

        As a style engine, I want to check if a specific DOM node matches a CSS selector (Tag, Class, ID).

        Acceptance: Returns true/false for a given Node vs Selector.

    Story 3.3: Style Computation (The Cascade)

        As a style engine, I want to compute the final set of properties for every DOM node before layout begins.

        Acceptance: ComputedStyle struct attached to every DOM node.

    Story 3.4: Layout Integration

        As a layout engine, I want to read dimensions (margin, padding, width) from the ComputedStyle rather than defaults.

        Acceptance: Changing CSS changes the visual spacing on screen.

Epic 4: The Navigator (Milestone 3)

Goal: Connect the engine to the real world.

    Story 4.1: The Networking Interface

        As a core engine, I need an INetwork interface to fetch resources asynchronously.

        Acceptance: INetwork::get(url, callback).

    Story 4.2: HTTP Implementation (cURL/Beast)

        As a platform maintainer, I want to implement INetwork using a library (e.g., libcurl or Boost.Beast).

        Acceptance: Fetch raw HTML from http://example.com.

    Story 4.3: The Browser Shell

        As a user, I want a UI overlay (URL bar) to input addresses.

        Acceptance: Typing a URL and hitting Enter triggers the download -> parse -> layout -> paint pipeline.

