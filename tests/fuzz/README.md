# Parser fuzzing (story 7.5.3)

libFuzzer harnesses for the HTML and CSS parsers, plus a cross-platform smoke
test that runs the same harness over the seed corpus in the normal test suite.

## Layout

- `FuzzTargets.{h,cpp}` — the shared "one input" bodies (`fuzz_html`, `fuzz_css`).
  The libFuzzer drivers and the smoke test both call these, so identical code is
  exercised locally and in CI.
- `html_parser_fuzzer.cpp` / `css_parser_fuzzer.cpp` — libFuzzer entry points.
- `ParserFuzzSmoke.test.cpp` — a gtest (part of `HummingbirdTests`) that feeds the
  corpus + adversarial snippets through the harness on every build, everywhere.
- `corpus/html/`, `corpus/css/` — seed inputs. **Check in any crash reproducer
  libFuzzer finds** so the smoke test locks the fix in (fix-forward policy).

## Running libFuzzer locally (clang)

```sh
cmake --preset ninja-multi-vcpkg -DHB_ENABLE_FUZZING=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build --preset ninja-multi-vcpkg --config Release \
  --target html_parser_fuzzer css_parser_fuzzer

# Short run (CI does ~45s each); drop -max_total_time for an open-ended run.
./build/tests/Release/html_parser_fuzzer -max_total_time=60 tests/fuzz/corpus/html
./build/tests/Release/css_parser_fuzzer  -max_total_time=60 tests/fuzz/corpus/css
```

`HB_ENABLE_FUZZING` instruments the whole build with coverage + AddressSanitizer
(clang only), so crashes inside the parsers are caught, not just in the harness.
The CI `fuzz` job runs the short pass on every push/PR and uploads any reproducer.
