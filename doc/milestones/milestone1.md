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

Epic 1.3: The Layout Engine (Expanded)

    Story 1.3.5: The "List" Simulation (MVP)

        As a reader,

        I want <ul> and <ol> items to look indented so I can distinguish them from normal text.

        Tech Strategy: Treat <ul> as a BlockBox with padding-left: 20px. Treat <li> as a standard BlockBox. Ignore the bullet points for now.

        Acceptance: The MFW "Design" section list is indented relative to the paragraphs.

    Story 1.3.6: Pre-formatted Text (<pre>)

        As a developer,

        I want code blocks to respect newlines and spaces in the source HTML.

        Tech Strategy: Add a whitespace_mode flag to RenderObject. If PRE, the layout engine does not collapse \n or multiple spaces into a single space.

        Acceptance: The code snippet on MFW renders with the correct indentation and line breaks.

    Story 1.3.7: The Anchor Tag (<a>) Visuals

        As a user,

        I want links to appear blue and underlined.

        Tech Strategy: In TreeBuilder, if node is <a>, set color = Blue and textDecoration = Underline on the created RenderObject.

        Acceptance: "Click here" text on MFW stands out.
    
    Story 1.3.8: The Heading Hierarchy (<h1> - <h6>)

        As a reader,

        I want to immediately distinguish sections based on font size and weight.

        Tech Strategy: In TreeBuilder, strictly map tags to these (approximate) scales relative to the base font:

            h1: 2.0em, Bold, Margin-y: 0.67em

            h2: 1.5em, Bold, Margin-y: 0.83em

            h3: 1.17em, Bold, Margin-y: 1.0em

            h4 - h6: Scala down to 1.0em or slightly smaller, but keep Bold.

        Acceptance: The MFW title is huge, subtitles are smaller, but all are distinct from body text.

    Story 1.3.9: Inline Code (<code> vs <pre>)

        As a developer reading MFW,

        I want inline references to HTML tags (like <div>) to look like code.

        Tech Strategy:

            Font Family: Set to Monospace (pass this enum to IGraphicsContext).

            Background: (Optional for MVP) Set a light gray background color on the TextBox.

            Difference from <pre>: <code> does not preserve whitespace by default and flows inline; <pre> is a block that preserves whitespace.

        Acceptance: Inline tags mentioned in the text appear in a monospace font.

    Story 1.3.10: The Blockquote (<blockquote>)

        As a reader,

        I want quoted text to be visually indented from the main content flow.

        Tech Strategy: Treat as a BlockBox with significantly larger default margins (e.g., margin-left: 40px, margin-right: 40px).

        Acceptance: The quote on MFW is centered/indented relative to the surrounding paragraphs.

    Story 1.3.11: The Horizontal Rule (<hr>)

        As a layout engine,

        I want to render a structural divider.

        Tech Strategy:

            This is a unique BlockBox that has no children.

            Height: Fixed (e.g., 2px).

            Width: Auto (matches parent).

            Style: Background color set to black/gray (since we don't have Borders yet).

            Margins: Default vertical spacing (e.g., 8px).

        Acceptance: A visible horizontal line separates sections of the page.

    Story 1.3.12: The Line Break (<br>)

        As a layout engine,

        I want to force the inline cursor to the next line immediately.

        Tech Strategy:

            This is not a "Box" to be drawn, but a "Control" instruction for the Layout cursor.

            When TreeBuilder sees <br>, it inserts a special RenderObject (e.g., RenderBreak).

            During layout(), if a RenderBreak is encountered, reset current_x to start_x and increment current_y by line_height.

        Acceptance: Text continues on the very next line without starting a new paragraph (no extra margin).

    Story 1.3.13: Semantic Emphasis (<em> and <strong>)

        As a reader,

        I want important text to be bold or italicized.

        Tech Strategy:

            <strong> maps to FontWeight::Bold.

            <em> maps to FontStyle::Italic.

            Ensure these nest correctly (e.g., bold-italic).

        Acceptance: Emphasis is visually distinct.

Epic 1.4: The Layout Engine (The "Flow")

Goal: Create RenderObjects from DOM nodes and calculate their (x, y, w, h).

    Story 1.4.1: Render Tree Construction

        As a layout engine,

        I want to walk the DOM and create a parallel tree of RenderObjects.

        Constraint: Only visible elements get a RenderObject (<head>, <script>, display:none do not).

        Acceptance: The TreeBuilder produces a root RenderBlock.

    Story 1.4.2: Default User Agent Styles (Hardcoded)

        As a renderer,

        I want specific C++ logic that dictates default appearance since we don't have CSS yet.

        Acceptance:

            h1: Font size 32px, Bold, Margin bottom.

            p: Block display.

            b / strong: Bold font weight.

            i / em: Italic style.

    Story 1.4.3: Block Layout (Vertical)

        As a layout engine,

        I want to implement BlockBox::layout().

        Logic:

            width: 100% of parent content box.

            x: Parent x.

            y: Parent y + sum(previous siblings height).

            height: Sum of children heights.

        Acceptance: Divs stack on top of each other.

    Story 1.4.4: Inline Layout (Horizontal Text)

        As a layout engine,

        I want to implement TextBox::layout().

        Logic:

            Calculate text width using IGraphicsContext::measureText().

            Advance the "cursor" x position for the next item.

            (MVP Simplification): No line-wrapping yet. If text is too long, it overflows.

        Acceptance: <span>A</span><span>B</span> renders as "AB" horizontally.

Epic 1.5: The Visualizer (The "Painter")

Goal: Execute draw commands based on the calculated layout.

    Story 1.5.1: The Paint Traversal

        As a painter,

        I want to recursively walk the Render Tree (src/renderer/Painter.cpp).

        Acceptance: Order is crucial: Background -> Border -> Children -> Text (Painter's Algorithm).

    Story 1.5.2: Debug Visualization

        As a developer,

        I want a "Debug Mode" toggle that draws colored outlines around every BlockBox.

        Acceptance: Pressing a key (e.g., F1) shows exact bounding boxes, helping debug layout math errors.

    Story 1.5.3: Text Rendering

        As a user,

        I want to see actual glyphs on the screen.

        Acceptance: The Painter calls IGraphicsContext::drawText() with the correct font size and position derived from the Layout phase.

Epic 1.6: Milestone Refactoring & Review

Goal: The clean-up phase discussed previously.

    Story 1.6.1: Memory Leak Audit

        As a developer,

        I want to run the demo with a heap profiler (or Valgrind/Visual Studio Diagnostics).

        Acceptance: Closing the window results in ArenaAllocator freeing all chunks; net memory change is zero.

    Story 1.6.2: File Structure Compliance

        As a lead,

        I want to ensure no src/layout code includes src/html headers directly (coupling only via src/core/dom).

        Acceptance: Strict separation of concerns.

Epic 1.7: Viewport & Overflow Handling

Goal: Ensure content adapts to the window width and allows the user to view content exceeding the window height.

    Story 1.7.1: Basic Text Wrapping (Greedy Algorithm)

        As a reader,

        I want sentences to break onto a new line when they hit the right edge of the window.

        Tech Strategy:

            Inside TextBox::layout():

            Split the text node string by spaces (basic tokenization).

            Loop through words: current_width += measure(word).

            Check: If current_x + current_width > parent_width:

                Reset current_x to parent_left.

                Increment current_y by font_height (line-height).

            Note: This is "Greedy" wrapping. Don't worry about "Knuth-Plass" balanced wrapping for MVP.

        Acceptance: Paragraphs on MFW form a nice column of text instead of one infinite line.

    Story 1.7.2: Vertical Scrolling (The Camera)

        As a user,

        I want to use the mouse wheel (or arrow keys) to move down the page.

        Tech Strategy:

            State: The App/Window needs a float scroll_y offset.

            Input: Catch SDL_MOUSEWHEEL in SDLWindow::pollEvents() and update scroll_y.

            Rendering: Pass this offset to the Painter. When drawing any box at (x, y), actually draw it at (x, y - scroll_y).

        Acceptance: I can scroll down to the bottom of MFW.

    Story 1.7.3: Viewport Clipping (Scissor Test)

        As a performance engineer,

        I want the engine to not try to draw text that is currently scrolled off-screen (above or below).

        Tech Strategy:

            Simple Bounding Box check in Painter:

            if (rect.y + rect.height < 0 || rect.y > window_height) return;

        Acceptance: Profiling shows we aren't issuing draw commands for the footer when we are looking at the header.