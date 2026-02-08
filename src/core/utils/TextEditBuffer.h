#pragma once

#include <string>
#include <string_view>

namespace Hummingbird::Core::Utils {

struct TextEditBuffer {
    static size_t clamp_caret_for(const std::string& text, size_t caret);
    static bool insert_text(std::string& text, size_t& caret, std::string_view insert_text);
    static bool backspace(std::string& text, size_t& caret);
    static bool delete_forward(std::string& text, size_t& caret);
    static void move_left(const std::string& text, size_t& caret);
    static void move_right(const std::string& text, size_t& caret);
    static void move_home(size_t& caret);
    static void move_end(const std::string& text, size_t& caret);
};

}  // namespace Hummingbird::Core::Utils
