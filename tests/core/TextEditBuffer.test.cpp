#include "core/utils/TextEditBuffer.h"

#include <gtest/gtest.h>

#include <string>

namespace {
using Hummingbird::Core::Utils::TextEditBuffer;

std::string utf8(const char8_t* text) {
    return std::string(reinterpret_cast<const char*>(text));
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
    std::string text = utf8(u8"aá😊");
    size_t caret = text.size();

    EXPECT_TRUE(TextEditBuffer::backspace(text, caret));
    EXPECT_EQ(text, utf8(u8"aá"));
    EXPECT_EQ(caret, utf8(u8"aá").size());
}

TEST(TextEditBufferTest, DeleteForwardRemovesNextCodepoint) {
    std::string text = utf8(u8"aá😊");
    size_t caret = utf8(u8"a").size();

    EXPECT_TRUE(TextEditBuffer::delete_forward(text, caret));
    EXPECT_EQ(text, utf8(u8"a😊"));
    EXPECT_EQ(caret, utf8(u8"a").size());
}

TEST(TextEditBufferTest, MoveLeftAndRightStepByCodepoint) {
    std::string text = utf8(u8"aá😊");
    size_t caret = text.size();
    size_t pos_acute = utf8(u8"a").size();
    size_t pos_emoji = utf8(u8"aá").size();

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
    std::string text = "ab";
    size_t caret = 1;

    EXPECT_TRUE(TextEditBuffer::insert_text(text, caret, utf8(u8"á")));
    EXPECT_EQ(text, utf8(u8"aáb"));
    EXPECT_EQ(caret, utf8(u8"aá").size());
}
