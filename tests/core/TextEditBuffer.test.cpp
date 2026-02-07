#include "core/utils/TextEditBuffer.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

namespace {
using Hummingbird::Core::Utils::TextEditBuffer;

std::string utf8_bytes(std::initializer_list<unsigned char> bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.begin()), bytes.size());
}
}  // namespace

TEST(TextEditBufferTest, ClampAndInsertAppend) {
    std::string text = "hi";
    size_t caret = 50;

    EXPECT_TRUE(TextEditBuffer::insert_text(text, caret, " there"));
    EXPECT_EQ(text, "hi there");
    EXPECT_EQ(caret, text.size());
}

TEST(TextEditBufferTest, BackspaceRemovesPreviousCodepoint) {
    const std::string a = "a";
    const std::string a_acute = utf8_bytes({0xC3, 0xA1});
    const std::string emoji = utf8_bytes({0xF0, 0x9F, 0x98, 0x8A});
    std::string text = a + a_acute + emoji;
    size_t caret = text.size();

    EXPECT_TRUE(TextEditBuffer::backspace(text, caret));
    EXPECT_EQ(text, a + a_acute);
    EXPECT_EQ(caret, (a + a_acute).size());
}

TEST(TextEditBufferTest, DeleteForwardRemovesNextCodepoint) {
    const std::string a = "a";
    const std::string a_acute = utf8_bytes({0xC3, 0xA1});
    const std::string emoji = utf8_bytes({0xF0, 0x9F, 0x98, 0x8A});
    std::string text = a + a_acute + emoji;
    size_t caret = a.size();

    EXPECT_TRUE(TextEditBuffer::delete_forward(text, caret));
    EXPECT_EQ(text, a + emoji);
    EXPECT_EQ(caret, a.size());
}

TEST(TextEditBufferTest, MoveLeftAndRightStepByCodepoint) {
    const std::string a = "a";
    const std::string a_acute = utf8_bytes({0xC3, 0xA1});
    const std::string emoji = utf8_bytes({0xF0, 0x9F, 0x98, 0x8A});
    std::string text = a + a_acute + emoji;
    size_t caret = text.size();
    size_t pos_acute = a.size();
    size_t pos_emoji = (a + a_acute).size();

    TextEditBuffer::move_left(text, caret);
    EXPECT_EQ(caret, pos_emoji);
    TextEditBuffer::move_left(text, caret);
    EXPECT_EQ(caret, pos_acute);
    TextEditBuffer::move_left(text, caret);
    EXPECT_EQ(caret, 0u);

    TextEditBuffer::move_right(text, caret);
    EXPECT_EQ(caret, pos_acute);
    TextEditBuffer::move_right(text, caret);
    EXPECT_EQ(caret, pos_emoji);
    TextEditBuffer::move_right(text, caret);
    EXPECT_EQ(caret, text.size());
}

TEST(TextEditBufferTest, InsertTextUpdatesCaretBytes) {
    const std::string a_acute = utf8_bytes({0xC3, 0xA1});
    std::string text = "ab";
    size_t caret = 1;

    EXPECT_TRUE(TextEditBuffer::insert_text(text, caret, a_acute));
    EXPECT_EQ(text, std::string("a") + a_acute + "b");
    EXPECT_EQ(caret, (std::string("a") + a_acute).size());
}
