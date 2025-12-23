This is an excellent idea. Establishing a "constitution" for your codebase early is the only way to keep a project of this complexity from collapsing under its own weight.

Below is a **Design & Architecture Guide** tailored specifically for **Hummingbird** (your engine). It combines general "Clean Code" principles with the specific C++20/Browser constraints we discussed.

You can save this as `DESIGN_GUIDE.md` in the root of your repository.

---

# Hummingbird Engine – Design & Contribution Guide

This document outlines the architectural principles, coding standards, and best practices for the Hummingbird browser engine. All code submitted to the project must adhere to these guidelines to ensure modularity, performance, and maintainability.

## 1. Architectural Principles

### 1.1. The "Ports & Adapters" Rule (Strict Modularity)

* **Core Logic is Sacred:** The `src/core`, `src/html`, and `src/layout` modules **must never** depend on `src/platform`.
* **Inversion of Control:** If the Core needs to draw pixels or fetch data, it must define an **Interface** (e.g., `IGraphicsContext`, `INetwork`). The Platform layer implements these interfaces.
* **Dependency Injection:** Do not instantiate concrete platform classes inside the core. Pass them in during initialization.

### 1.2. The Single-Threaded Main Loop

* **Main Thread:** DOM manipulation, Layout calculation, and Command Recording happen strictly on the main thread.
* **Asynchrony:** Never block the main thread for I/O.
* **Bad:** `auto data = file.read();` (Blocks UI)
* **Good:** `file.readAsync([](auto data) { ... });` (Callback/Event based)



---

## 2. Memory Management

### 2.1. The Three Tiers of Allocation

1. **Arena (The default for Data):**
* **Use for:** DOM Nodes, Layout Boxes, CSS Rules, Tokens.
* **Why:** High frequency allocation, zero fragmentation, instant deallocation on page refresh.
* **Rule:** These objects should generally have **no virtual destructors** (unless absolutely necessary) and are effectively "POD" (Plain Old Data) types.


2. **`std::unique_ptr` (The default for Systems):**
* **Use for:** Managers, Parsers, Platform Wrappers.
* **Rule:** Explicit ownership.


3. **Raw Pointers (Observers):**
* **Use for:** Passing non-owning references to functions (e.g., `Node* parent`).
* **Rule:** Never call `delete` on a raw pointer.



### 2.2. String Hygiene

* **Input Parameters:** Always use `std::string_view` for read-only string arguments.
* *Bad:* `void parse(std::string text)` (Copies memory)
* *Good:* `void parse(std::string_view text)` (Zero copy)


* **Storage:** Only convert to `std::string` when the object needs to own the data (e.g., a Text Node in the DOM).

---

## 3. Function & Class Design

### 3.1. Parameter Objects

If a method requires more than **3 arguments**, group them into a context/configuration struct.

**Bad:**

```cpp
void layout(Node* node, int x, int y, int w, int h, bool isFlex, Font* font);

```

**Good:**

```cpp
struct LayoutContext {
    Rect bounds;
    bool isFlexMode;
    Font* font;
};

void layout(Node* node, const LayoutContext& ctx);

```

### 3.2. Single Level of Abstraction (SLAP)

Functions should read like a story. Don't mix high-level logic with low-level bit-shifting.

**Bad:**

```cpp
void render() {
    // High level
    layoutTree();
    // Low level mixed in
    int* pixel = (int*)buffer;
    *pixel = (r << 24) | (g << 16); 
}

```

**Good:**

```cpp
void render() {
    layoutTree();
    paintPixels(); // Encapsulates the bit-shifting logic
}

```

### 3.3. Method Length

* **Target:** < 40 lines.
* **Reason:** If it's longer, it's likely doing too many things. Break it down into private helper methods.

### 3.4. const Correctness

* Mark methods `const` if they do not modify the object state.
* Mark parameters `const` if they are not modified.
* **Why:** This allows the compiler to optimize better and prevents silly bugs.

---

## 4. Modern C++ (C++20) Standards

### 4.1. Concepts over `typename`

Use C++20 **Concepts** to constrain templates. This produces readable error messages.

**Bad:**

```cpp
template<typename T>
void draw(T object) { ... }

```

**Good:**

```cpp
template<IsRenderable T>
void draw(T object) { ... }

```

### 4.2. Header Hygiene

* **`#pragma once`:** Use in every header.
* **Forward Declarations:** Avoid `#include` in header files whenever possible. Use forward declarations (`class Node;`) to reduce compilation dependency chains.
* **Modules:** (Optional) If the compiler support is stable, prefer C++20 Modules (`import`) over headers.

### 4.3. Type Safety

* **`auto`:** Use `auto` when the type is obvious (`auto* node = new Node();`) or purely redundant (`std::vector<int>::iterator`). Do *not* use `auto` when the type is unclear.
* **`enum class`:** Always use `enum class` instead of C-style `enum` to prevent namespace pollution.

---

## 5. Error Handling

### 5.1. No Exceptions (Mostly)

Browser engines are performance-critical. Avoid `try/catch` in hot paths (parsing, layout).

* **Alternative:** Use `std::optional<T>` or a custom `Result<T, Error>` type for operations that might fail.
* **Fatal Errors:** For unrecoverable state (e.g., out of memory), fast-fail (`std::terminate` or `abort`).

### 5.2. Logging

* Use the internal `Core::Log` macros.
* **`LOG_INFO`**: High-level lifecycle events.
* **`LOG_WARN`**: Recoverable issues (e.g., "Unknown HTML tag").
* **`LOG_ERROR`**: Something broke, but we are limping along.

---

## 6. Directory Structure & Naming

### 6.1. File Naming

* **PascalCase** for files and classes: `HtmlParser.cpp`, `BlockBox.h`.
* **Classes** match filenames: `class HtmlParser` lives in `HtmlParser.h`.

### 6.2. Variable Naming

* **`m_variable`**: Private member variables.
* **`variable`**: Local variables / Function arguments.
* **`kConstant`**: Global/Static constants.
* **`ClassName`**: Types.

### 6.3. Namespace

All code must reside in the `Hummingbird` namespace, with sub-namespaces for modules:

* `Hummingbird::Core`
* `Hummingbird::Html`
* `Hummingbird::Layout`
* `Hummingbird::Platform`

---

## 7. Review Checklist (Self-Correction)

Before committing, ask:

1. **Isolation:** Did I include a Platform header in Core? (If yes, Refactor).
2. **Memory:** Am I using `new` without an Arena? (If yes, Justify).
3. **Clarity:** Can a junior dev understand this function without reading the comments?
4. **Tests:** Did I break the "MotherfuckingWebsite" golden master?