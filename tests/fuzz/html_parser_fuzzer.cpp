#include <cstddef>
#include <cstdint>

#include "fuzz/FuzzTargets.h"

// libFuzzer entry point for the HTML parser. Build with a fuzzer-capable
// compiler via -DHB_ENABLE_FUZZING=ON (clang -fsanitize=fuzzer,address).
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Hummingbird::Fuzz::fuzz_html(data, size);
    return 0;
}
