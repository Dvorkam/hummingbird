### **Milestone 2: The Stylist (Backlog)**

**Theme:** Separation of Concerns (CSS Architecture).
**Goal:** Parse external CSS, compute the "Cascade" (winning styles), and update the Layout/Paint engines to respect these values.

#### **Epic 2.1: The CSS Parser (Zero-Copy)**

**Goal:** Turn raw CSS text into a `Stylesheet` object stored in the Arena.

* **Story 2.1.1: CSS Tokenizer**
* **As a** parser,
* **I want** to break a CSS string into tokens (`Ident`, `Colon`, `Semicolon`, `LBrace`, `RBrace`) using `std::string_view`.
* **Acceptance:** `body { color: red; }` becomes `[Ident:body] [LBrace] [Ident:color] [Colon] [Ident:red] [Semicolon] [RBrace]`.


* **Story 2.1.2: The Rule Parser**
* **As a** developer,
* **I want** to group tokens into `Rule` objects (Selectors + Declarations block).
* **Acceptance:** The parser outputs a `CSS::Rule` containing a `Selector` list and a `Declaration` list.


* **Story 2.1.3: Property & Value Parsing**
* **As a** style engine,
* **I want** to map string values (`"red"`, `"12px"`) to strongly typed enums/unions (`Color::Red`, `Length(12, Px)`).
* **Acceptance:** `margin: 10px` is stored as `{ Property::Margin, Value(10, Unit::Px) }`.

* **Story 2.1.4: Inline `<style>` Extraction**
* **As a** style engine,
* **I want** to extract CSS text from `<style>` tags in HTML and feed it into the CSS parser.
* **Acceptance:** A document containing `<style>body { color: red; }</style>` yields a stylesheet that can apply to DOM nodes.



#### **Epic 2.2: The Style Resolver (The "Cascade")**

**Goal:** Determine exactly which style applies to which DOM node.

* **Story 2.2.1: Basic Selector Matching**
* **As a** style engine,
* **I want** to match DOM nodes against Type (`div`), Class (`.container`), and ID (`#main`) selectors.
* **Acceptance:** `SelectorMatcher::match(node, selector)` returns `true` if the node has the matching tag/class/id.


* **Story 2.2.2: Specificity Calculation**
* **As a** style engine,
* **I want** to assign a weight to every matching rule so that IDs beat Classes, and Classes beat Tags.
* **Acceptance:** A node matching both `p { color: red; }` and `.text { color: blue; }` results in Blue.


* **Story 2.2.3: ComputedStyle Generation**
* **As a** layout engine,
* **I want** every DOM node to have a `ComputedStyle` struct attached to it *before* layout begins.
* **Acceptance:** `Node::computedStyle()` contains the final resolved values for display, color, margin, etc.



#### **Epic 2.3: Box Model Refinement**

**Goal:** Upgrade the Layout engine to handle the full CSS Box Model (Margins, Borders, Padding).

* **Story 2.3.1: Variable Margins & Padding**
* **As a** layout engine,
* **I want** to read `margin-left` or `padding-top` from the `ComputedStyle` instead of hardcoded defaults.
* **Acceptance:** A `div` with `padding: 50px` visibly pushes its children inward.


* **Story 2.3.2: Border Rendering**
* **As a** painter,
* **I want** to draw borders around boxes based on `border-width`, `border-color`, and `border-style`.
* **Acceptance:** A red, 1px solid border appears around elements defined in CSS.


* **Story 2.3.3: `display: none` Support**
* **As a** layout engine,
* **I want** to completely skip generating `RenderObject`s for nodes with `display: none`.
* **Acceptance:** Elements hidden via CSS do not take up any space in the layout.

* **Story 2.3.4: Inline Line Builder**
* **As a** layout engine,
* **I want** inline content to be flattened into shared line boxes at the paragraph level instead of wrapping per inline node.
* **Acceptance:** Inline text and inline elements wrap as a single flow; line breaks are determined across sibling inline runs.


* **Story 2.3.5: Inline Run Measurement**
* **As a** layout engine,
* **I want** inline elements to contribute styled runs (text + metrics) to the line builder rather than owning their own wrapping.
* **Acceptance:** `<a>`, `<em>`, `<code>` runs are measured in the same line context and share a consistent baseline/line height.


* **Story 2.3.6: Inline Layout & Paint Integration**
* **As a** renderer,
* **I want** inline line boxes to drive final positions for text and inline boxes so rendering matches the shared line layout.
* **Acceptance:** Inline boxes paint at their line-builder positions without resetting x/y to their own origin.


* **Story 2.3.7: Inline Wrapping Regression Tests**
* **As a** developer,
* **I want** tests that cover inline wrapping across style boundaries so regressions are caught early.
* **Acceptance:** A paragraph with mixed `<a>`, `<em>`, `<code>` wraps without restarting at the inline element's origin.



#### **Epic 2.4: The "Real" List Item (Fixing the M1 Cheat)**

**Goal:** Replace the "margin hack" from Milestone 1 with actual list logic.

* **Story 2.4.1: `display: list-item` & The Marker**
* **As a** layout engine,
* **I want** elements with `display: list-item` to generate a secondary "Marker" box (the bullet point) automatically.
* **Tech Strategy:** The `RenderListItem` (subclass of Block) owns a child `RenderMarker`.
* **Acceptance:** `<ul>` items have real bullet points that are positioned to the left of the content.



### **Epic 2.5: Architecture Hardening & Tech Debt Paydown**

**Goal:** Ensure the codebase adheres to "Ports & Adapters," has zero "spaghetti dependencies," and is self-documenting before Milestone 3 begins.

#### **Story 2.5.1: The Architectural Firewall (Dependency Audit)**

* **As a** Lead Architect,
* **I want** to physically prevent the `Core` and `Layout` modules from seeing `Platform` headers.
* **The Problem:** It's too easy to accidentally `#include <SDL.h>` in `RenderObject.cpp` to quickly check a key press. This breaks portability.
* **Action:**
* Review all `#include` directives in `src/core`, `src/html`, `src/style`, and `src/layout`.
* If a platform header is found, create an abstract interface in `src/core/platform_api` (e.g., `ITimeSource`, `ILogger`) and move the implementation to `src/platform`.


* **Acceptance:**
* Grepping for "SDL" or "curl" in `src/core` returns 0 results.
* The build system (CMake) explicitly forbids linking `Core` against `SDL2`.



#### **Story 2.5.2: The "Big Method" Split & Abstraction Hygiene**

* **As a** Maintainer,
* **I want** to split massive "god functions" into small, single-purpose methods **and** enforce consistent levels of abstraction within each function.
* **Level A — God Functions:**
* `HtmlParser::parse()`: Likely a giant loop. Split into `parseStartTag()`, `parseEndTag()`, `handleCharacterData()`.
* `TreeBuilder::createRenderObject()`: Likely a massive `if/else` chain for every tag.
* **Level B — Abstraction Hygiene:**
* If a function calls higher-level helpers, it must not contain low-level parsing/layout details.
* Extract low-level work into named helpers so each function reads at a single abstraction level.


* **Action:**
* Apply the **"Extract Method"** refactoring pattern.
* Ensure no function exceeds ~30-50 lines of code.
* Enforce SLAP: high-level methods delegate; low-level methods do the mechanics.


* **Acceptance:**
* `TreeBuilder.cpp` reads like a high-level table of contents, not a wall of logic.
* Functions do not mix high-level orchestration with low-level mechanics.

#### **Story 2.5.3: Magic String & Magic Number Purge**

* **As a** Developer,
* **I want** to replace raw strings (`"div"`, `"margin"`) and numbers (`0xFF0000`) with named Constants and Enums.
* **The Problem:** Typing `"backgroud-color"` (typo) in three different places causes silent bugs.
* **Action:**
* Create `HtmlTagNames.h` (using `static constexpr std::string_view`).
* Create `CssPropertyNames.h`.
* Replace hardcoded pixel values in Layout with named constants like `kDefaultLineHeight`.


* **Acceptance:** The string `"div"` appears in exactly one place in the entire codebase (the constant definition).

#### **Story 2.5.4: The Factory Pattern (Centralized Object Creation)**

* **As a** Memory Safe Architect,
* **I want** to forbid direct calls to `new BlockBox()` or `new Element()` scattered throughout the code.
* **Action:**
* Make constructors for `Node`, `Element`, `RenderObject` **protected/private**.
* Force all creation through specific "Factory" methods (e.g., `DomFactory::createElement(tag, arena)`) or `friend` classes (like `TreeBuilder`).
* This ensures the `ArenaAllocator` is *always* used and we never accidentally create a node on the standard heap.


* **Acceptance:** Calling `new Element(...)` in `main.cpp` creates a compiler error.

#### **Story 2.5.5: Const Correctness & View Safety**

* **As a** Safety Engineer,
* **I want** to ensure we aren't modifying data that should be read-only, and that our `string_views` are safe.
* **Action:**
* Audit all getters: `getStyle()` should return `const ComputedStyle&`, not a mutable reference/copy.
* **Lifetime Audit:** Verify that the `std::string` holding the HTML source code *strictly outlives* every `std::string_view` in the DOM.


* **Acceptance:**
* Compiler errors if logic tries to modify a `Node` during the `Painting` phase (which should be read-only).



#### **Story 2.5.6: Unified Logging & Error Facade**

* **As a** Developer,
* **I want** a standardized way to scream for help when things break.
* **Action:**
* Replace raw `std::cerr` or `printf` with the macros defined in `core/utils/Log.h` (`LOG_INFO`, `LOG_WARN`).
* Ensure every "TODO" or "Hack" has a `LOG_WARN("Not implemented: ...")` so the console output serves as a roadmap.


* **Acceptance:** Running the browser produces a clean, categorized log stream (e.g., `[RENDER] [WARN] Tag <video> not supported`).

---

### **Epic 2.6: Stylesheet Sources & Loading (Offline / Harness)**

**Goal:** Make “pages render differently based on a .css file” concretely true without HTTP.

#### **Story 2.6.1: `<link rel="stylesheet">` Discovery**
* **As a** style engine,
* **I want** to extract stylesheet hrefs from the DOM (head, and optionally body).
* **Acceptance:** DOM containing `<link rel="stylesheet" href="site.css">` yields a recorded stylesheet request in document order.

#### **Story 2.6.2: `IResourceProvider` Boundary (No Platform Headers)**
* **As a** lead architect,
* **I want** Core/Html/Layout to request resources through a Core interface, not Platform headers.
* **Action:**
* Add `Core` interface: `IResourceProvider::load_text(ResourceId)` (returns optional buffer/view).
* App/Platform supplies a file-based implementation for Milestone 2.
* **Acceptance:** Core/Html/Layout compile without Platform deps; stylesheet text is delivered through the interface.

#### **Story 2.6.3: Author Stylesheet Merge + Cascade Order**
* **As a** style engine,
* **I want** author stylesheets to be merged in correct order before applying the cascade.
* **Order:**
* UA defaults (can remain hardcoded or represented as a built-in stylesheet string).
* `<link>` stylesheets in document order.
* `<style>` blocks in document order.
* **Acceptance:** Later rules override earlier rules when specificity ties.



### **Epic 2.7: Color & Background Paint Integration**

**Goal:** Ensure parsed/computed styles affect pixels.

#### **Story 2.7.1: Typed Color Value Parsing**
* **As a** CSS parser,
* **I want** named colors and hex colors to map to typed values.
* **Support:** `red`, `black`, `white`, `#rgb`, `#rrggbb`.
* **Acceptance:** `color: #333; background-color: white;` produces correct typed values.

#### **Story 2.7.2: `color` Applied to Text**
* **As a** renderer,
* **I want** text painting to use `ComputedStyle.color` (not hardcoded defaults).
* **Acceptance:** `p { color: red; }` makes paragraph text red.

#### **Story 2.7.3: `background-color` Applied to Boxes**
* **As a** renderer,
* **I want** box backgrounds filled before children where applicable.
* **Acceptance:** `div { background-color: #eee; }` visibly fills behind children.



### **Epic 2.8: `<img>` as a Replaced Element (Placeholder MVP)**

**Goal:** Support basic images without a full media pipeline.

#### **Story 2.8.1: Intrinsic Sizing (Attributes-First)**
* **As a** layout engine,
* **I want** `<img>` to use width/height attributes if present.
* **Fallback:** Use a conservative default (e.g., 300x150).
* **Acceptance:** `<img width="64" height="64">` occupies 64x64 in layout.

#### **Story 2.8.2: Inline Integration**
* **As a** layout engine,
* **I want** `<img>` to participate in inline flow (like inline-block).
* **Acceptance:** Text `<img ...> Text` lays out on one line until wrapping.

#### **Story 2.8.3: Placeholder Painting**
* **As a** renderer,
* **I want** a visible placeholder box with optional alt text.
* **Acceptance:** Images “exist” visually and do not collapse layout even without decoding.

---

### **Epic 2.9: Table Layout Scaffolding (MVP Grid)**

**Goal:** Provide a minimal table layout model that aligns rows/cells for classic HTML pages.

#### **Story 2.9.1: Table Render Objects**
* **As a** layout engine,
* **I want** `<table>`, `<tr>`, `<td>`, `<th>` to map to dedicated render objects (e.g., RenderTable, RenderTableRow, RenderTableCell).
* **Acceptance:** A DOM table produces a render tree that preserves row/cell structure.

#### **Story 2.9.2: Two-Pass Table Measurement**
* **As a** layout engine,
* **I want** to measure cell contents, compute column widths, then assign row heights.
* **Acceptance:** Two cells in a row align horizontally and share a common row height.

#### **Story 2.9.3: Table Width Resolution**
* **As a** layout engine,
* **I want** tables to honor explicit widths (including `width="100%"`) and distribute extra space across columns.
* **Acceptance:** A 100% width table spans the parent width and columns expand accordingly.

#### **Story 2.9.4: Basic `colspan` Support**
* **As a** layout engine,
* **I want** `colspan` to let a cell span multiple columns (no complex edge cases yet).
* **Acceptance:** `<td colspan="2">` occupies the combined width of two columns.



### **Epic 2.10: Legacy Attribute Translation (HTML -> CSS)**

**Goal:** Map presentational HTML attributes to computed style so classic pages render sensibly.

#### **Story 2.10.1: `align` Attribute Mapping**
* **As a** style engine,
* **I want** `align="left|center|right"` on table cells/rows to map to `text-align`.
* **Acceptance:** `<td align="center">` centers its inline content.

#### **Story 2.10.2: `nowrap` Attribute Mapping**
* **As a** style engine,
* **I want** `nowrap` to map to a no-wrap white-space mode.
* **Acceptance:** Cells with `nowrap` keep inline content on one line when possible.

#### **Story 2.10.3: Width/Height Attribute Mapping**
* **As a** style engine,
* **I want** `width`/`height` attributes to map to computed width/height when CSS has not set them.
* **Acceptance:** `<table width="100%">` and `<td width="200">` influence layout.

#### **Story 2.10.4: `<font>` Tag Translation (Scoped MVP)**
* **As a** style engine,
* **I want** `<font size="N">` to map to computed font size (face handling can be minimal).
* **Acceptance:** `<font size="6">` produces visibly larger text than surrounding content.



### **Epic 2.11: Text Alignment & No-Wrap Support**

**Goal:** Allow alignment and wrapping control to influence inline layout.

#### **Story 2.11.1: `text-align` Property**
* **As a** layout engine,
* **I want** `text-align: left|center|right` to offset inline line boxes within the available width.
* **Acceptance:** A centered paragraph lays out with line boxes centered in its container.

#### **Story 2.11.2: `white-space: nowrap`**
* **As a** layout engine,
* **I want** `nowrap` to prevent line wrapping for inline runs.
* **Acceptance:** A `nowrap` cell lays out a single line unless it exceeds the container width.

---

### **Refactoring Checklist (The "Definition of Done" for Epic 2.5)**

Before you write a single line of CSS parsing code, the codebase must pass this "Health Check":

1. **No Warnings:** The project compiles with `-Wall -Wextra` (or MSVC level 4) with zero warnings.
2. **Zero Leaks:** Running the "Hello World" HTML demo under a sanitizer (ASan) shows 0 bytes lost on shutdown.
3. **Header Hygiene:** `Core` headers do not include `Platform` headers.
4. **Formatting:** `clang-format` has been applied to all files (consistency).


---

### **5 Reference Websites (HTML + CSS Only)**

These sites are perfect "Golden Masters" for Milestone 2 because they rely heavily on the **Cascade**, **Box Model**, and **Typography** without needing JavaScript.

1. **[IANA Example Domains](https://www.iana.org/help/example-domains)**
* **Why:** A clean, stable HTML page that’s safe to link publicly.
* **Test Value:** Basic typography + link styling without relying on scripting.


2. **[CSS Zen Garden (The Sample HTML)](http://www.csszengarden.com/)**
* **Why:** The *ultimate* test of CSS. The HTML never changes, but the CSS radically alters the layout.
* **Test Value:** If you can load the default "Zen Garden" CSS and it looks decent (even without complex floats/grid), your Box Model and Selector engine are solid.


3. **[Tacit CSS Project](https://yegor256.github.io/tacit/)**
* **Why:** A "Classless" CSS framework. It styles raw HTML tags to look beautiful without using any `.class` or `#id`.
* **Test Value:** Perfect for testing your **Element Type Selector** logic (`nav`, `section`, `form`, `button`) and descendant selectors.


4. **[Berkshire Hathaway](https://www.berkshirehathaway.com/)**
* **Why:** The most famous "brutalist" corporate site. It is almost pure HTML with extremely minimal styling.
* **Test Value:** A great "Regression Test" to ensure simple layouts don't break as you add complex CSS logic.


5. **[The W3C Core Styles](https://www.w3.org/StyleSheets/Core/)**
* **Why:** This is the W3C's own set of "default" stylesheets (Chocolate, Midnight, Steely, etc.) from the late 90s.
* **Test Value:** They use very standard, compliant CSS 1.0/2.1 features. If you can render these, you are practically a generic CSS 2.1 browser.
