Definition of Done (Milestone 1)

    Input: A raw HTML string containing nested tags (<div>, <p>), headings (<h1>), formatting (<b>), and void tags (<img>, <br>).

    Output: A window displaying the text laid out vertically (blocks) and horizontally (text), with basic default styling (bolding, font sizes).

    Observability: Any HTML tag not explicitly handled is logged to std::cerr or a log file as "TODO: [TagName]", but does not crash the engine.

Epic 1.1: The HTML Tokenizer (The "Lexer")

Goal: robustly chop raw strings into HtmlToken objects without memory allocations for substrings.

    Story 1.1.1: State Machine Basics

        As a parser,

        I want to implement a state machine that switches between Data (text), TagOpen (<), TagName (div), and SelfClosingStartTag (/>).

        Acceptance: Tokenizer::next() correctly identifies start and end tags for <div> and </div>.

    Story 1.1.2: Zero-Copy Tokens

        As a performance engineer,

        I want HtmlToken to use std::string_view pointing to the original source buffer instead of std::string.

        Acceptance: sizeof(HtmlToken) is small (struct of 2-3 views/enums), and creating tokens involves zero heap allocations.

    Story 1.1.3: Attribute Parsing

        As a developer,

        I want to parse id="main" class="container" inside tags.

        Acceptance: The HtmlToken struct contains a list (or span) of attribute pairs.

Epic 1.2: The DOM Builder (The "Parser")

Goal: Construct the tree of Node objects in src/core/dom using the ArenaAllocator.

    Story 1.2.1: The Open Element Stack

        As a parser,

        I want to maintain a stack of open elements so that when I see </div>, I close the most recent <div>.

        Acceptance: Nested structures <div><p></p></div> result in p being a child of div.

    Story 1.2.2: Void Element Handling

        As a parser,

        I want to identify "void" elements (<img>, <br>, <hr>, <input>) and not push them onto the open stack.

        Acceptance: <br> creates a node but does not capture subsequent text as its children.

    Story 1.2.3: The "Unsupported Tag" Catcher (The TBD List)

        As a developer,

        I want to log a warning whenever a tag is encountered that is not in my "Known Tags" whitelist (e.g., <video>, <table>).

        Acceptance:

            The unknown tag is still added to the DOM as a generic Element (prevents data loss).

            Console prints: [WARN] Unsupported HTML Tag encountered: <video> (deduplicated logging preferred).

    Story 1.2.4: Text Node Coalescing

        As a memory optimizer,

        I want to merge adjacent text tokens into a single Text node.

        Acceptance: Hello + + World becomes one DOM node Hello World, not three.

Epic 1.3: The Layout Engine (The "Flow")

Goal: Create RenderObjects from DOM nodes and calculate their (x, y, w, h).

    Story 1.3.1: Render Tree Construction

        As a layout engine,

        I want to walk the DOM and create a parallel tree of RenderObjects.

        Constraint: Only visible elements get a RenderObject (<head>, <script>, display:none do not).

        Acceptance: The TreeBuilder produces a root RenderBlock.

    Story 1.3.2: Default User Agent Styles (Hardcoded)

        As a renderer,

        I want specific C++ logic that dictates default appearance since we don't have CSS yet.

        Acceptance:

            h1: Font size 32px, Bold, Margin bottom.

            p: Block display.

            b / strong: Bold font weight.

            i / em: Italic style.

    Story 1.3.3: Block Layout (Vertical)

        As a layout engine,

        I want to implement BlockBox::layout().

        Logic:

            width: 100% of parent content box.

            x: Parent x.

            y: Parent y + sum(previous siblings height).

            height: Sum of children heights.

        Acceptance: Divs stack on top of each other.

    Story 1.3.4: Inline Layout (Horizontal Text)

        As a layout engine,

        I want to implement TextBox::layout().

        Logic:

            Calculate text width using IGraphicsContext::measureText().

            Advance the "cursor" x position for the next item.

            (MVP Simplification): No line-wrapping yet. If text is too long, it overflows.

        Acceptance: <span>A</span><span>B</span> renders as "AB" horizontally.

Epic 1.4: The Visualizer (The "Painter")

Goal: Execute draw commands based on the calculated layout.

    Story 1.4.1: The Paint Traversal

        As a painter,

        I want to recursively walk the Render Tree (src/renderer/Painter.cpp).

        Acceptance: Order is crucial: Background -> Border -> Children -> Text (Painter's Algorithm).

    Story 1.4.2: Debug Visualization

        As a developer,

        I want a "Debug Mode" toggle that draws colored outlines around every BlockBox.

        Acceptance: Pressing a key (e.g., F1) shows exact bounding boxes, helping debug layout math errors.

    Story 1.4.3: Text Rendering

        As a user,

        I want to see actual glyphs on the screen.

        Acceptance: The Painter calls IGraphicsContext::drawText() with the correct font size and position derived from the Layout phase.

Epic 1.5: Milestone Refactoring & Review

Goal: The clean-up phase discussed previously.

    Story 1.5.1: Memory Leak Audit

        As a developer,

        I want to run the demo with a heap profiler (or Valgrind/Visual Studio Diagnostics).

        Acceptance: Closing the window results in ArenaAllocator freeing all chunks; net memory change is zero.

    Story 1.5.2: File Structure Compliance

        As a lead,

        I want to ensure no src/layout code includes src/html headers directly (coupling only via src/core/dom).

        Acceptance: Strict separation of concerns.