# CSS Property Workflow

This guide defines how to add CSS properties after the Phase 4 registry refactor.

## 1) Fast path: reuse existing hooks

If the new property uses existing parse/apply behavior:

1. Add one entry in `src/style/registry/CssPropertyList.inl`:
   - `Property` id
   - CSS name
   - canonical name
   - `ParserHook`
   - `ApplyHook`
   - flags
2. Add tests for expected behavior.
3. Run build + tests.

No extra enum wiring or string map wiring should be needed.

## 2) Add a new parser behavior

Only if no current parser hook matches:

1. Add a new `ParserHook` value in `src/style/registry/CssPropertyList.h`.
2. Implement handling in `src/style/parser/CssParser.cpp`.
3. Reference the new hook in the property entry in `src/style/registry/CssPropertyList.inl`.
4. Add parser tests.

## 3) Add a new apply behavior

Only if no current applier hook matches:

1. Add a new `ApplyHook` value in `src/style/registry/CssPropertyList.h`.
2. Wire dispatch in `src/style/compute/apply/PropertyApplier.cpp`.
3. Implement behavior in the relevant apply module (`ApplyLayout.cpp`, `ApplyText.cpp`, `ApplyBackground.cpp`, or a new apply module).
4. Reference the new hook in `src/style/registry/CssPropertyList.inl`.
5. Add style/layout tests.

## 4) Invariants and safety checks

Registry metadata is validated by compile-time checks in `src/style/registry/CssPropertyList.h`:

- no duplicate property names
- one canonical entry per property
- aliases point to canonical entries of the same property
- hooks are not `Unknown`

Registry sanity is also covered by `tests/style/CssPropertyRegistry.test.cpp`.

## 5) Validation commands

```bash
clang-format -i -- src/**/*.cpp src/**/*.h tests/**/*.cpp tests/**/*.h
cmake --build --preset user-ninja-multi-vcpkglt
ctest --preset user-ninja-multi-vcpkglt
```
