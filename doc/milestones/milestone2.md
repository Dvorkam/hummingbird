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



#### **Epic 2.5: Milestone Refactoring**

**Goal:** Code hygiene.

* **Story 2.5.1: The CSS/Layout Wall**
* **As a** architect,
* **I want** to ensure `src/layout` never calls the CSS Parser directly. It should only read the `ComputedStyle`.
* **Acceptance:** Strict header separation.


* **Story 2.5.2: Arena Optimization for Styles**
* **As a** memory optimizer,
* **I want** CSS Rules and Declarations to live in the `Arena`.
* **Acceptance:** No `new` calls in `CssParser.cpp`.



---

### **5 Reference Websites (HTML + CSS Only)**

These sites are perfect "Golden Masters" for Milestone 2 because they rely heavily on the **Cascade**, **Box Model**, and **Typography** without needing JavaScript.

1. **[BetterMotherfuckingWebsite.com](http://bettermotherfuckingwebsite.com/)**
* **Why:** This is the direct sequel to your M1 test case. It adds basic CSS (margins, line-height, color, max-width) to the original.
* **Test Value:** Verifies that your engine can override the "User Agent" styles you wrote in M1.


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
