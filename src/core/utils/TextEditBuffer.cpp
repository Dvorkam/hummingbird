#include "core/utils/TextEditBuffer.h"

#include "core/utils/Utf8Utils.h"

namespace Hummingbird::Core::Utils {

size_t TextEditBuffer::clamp_caret_for(const std::string& text, size_t caret) {
    return clamp_caret(caret, text);
}

bool TextEditBuffer::insert_text(std::string& text, size_t& caret, std::string_view insert_text) {
    if (insert_text.empty()) {
        return false;
    }
    caret = clamp_caret(caret, text);
    text.insert(caret, insert_text);
    caret += insert_text.size();
    return true;
}

bool TextEditBuffer::backspace(std::string& text, size_t& caret) {
    if (text.empty()) {
        return false;
    }
    caret = clamp_caret(caret, text);
    if (caret == 0) {
        return false;
    }
    auto start = prev_codepoint(text, caret);
    text.erase(start, caret - start);
    caret = start;
    return true;
}

bool TextEditBuffer::delete_forward(std::string& text, size_t& caret) {
    if (text.empty()) {
        return false;
    }
    caret = clamp_caret(caret, text);
    if (caret >= text.size()) {
        return false;
    }
    auto end = next_codepoint(text, caret);
    text.erase(caret, end - caret);
    return true;
}

void TextEditBuffer::move_left(const std::string& text, size_t& caret) {
    caret = clamp_caret(caret, text);
    caret = prev_codepoint(text, caret);
}

void TextEditBuffer::move_right(const std::string& text, size_t& caret) {
    caret = clamp_caret(caret, text);
    caret = next_codepoint(text, caret);
}

void TextEditBuffer::move_home(size_t& caret) {
    caret = 0;
}

void TextEditBuffer::move_end(const std::string& text, size_t& caret) {
    caret = text.size();
}

}  // namespace Hummingbird::Core::Utils
