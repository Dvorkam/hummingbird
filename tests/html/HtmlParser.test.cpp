#include "html/HtmlParser.h"

#include <gtest/gtest.h>

#include <functional>

#include "html/HtmlTagNames.h"
#include "style/registry/CssPropertyNames.h"

using namespace Hummingbird::Html;
namespace TagNames = Hummingbird::Html::TagNames;
namespace PropertyNames = Hummingbird::Css::PropertyNames;

TEST(HtmlParserTest, SimpleTreeConstruction) {
    std::string_view html = "<html><body><p>Hello</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(1024);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto html_node = result.dom->get_children()[0].get();
    ASSERT_EQ(html_node->get_children().size(), 1u);
    auto body_node = html_node->get_children()[0].get();
    ASSERT_EQ(body_node->get_children().size(), 1u);
}

TEST(HtmlParserTest, CoalescesAdjacentTextNodes) {
    std::string_view html = "<div>Hello <!--comment-->World</div>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    ASSERT_EQ(div_node->get_children().size(), 1u);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(div_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "Hello World");
}

TEST(HtmlParserTest, HandlesVoidAndSelfClosingTagsWithoutStackingChildren) {
    std::string_view html = "<div>Hello<br/>World<img src='x'/></div>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    const auto& children = div_node->get_children();
    ASSERT_EQ(children.size(), 4u);
    auto leading_text = dynamic_cast<Hummingbird::DOM::Text*>(children[0].get());
    ASSERT_NE(leading_text, nullptr);
    EXPECT_EQ(leading_text->get_text(), "Hello");

    auto br_node = dynamic_cast<Hummingbird::DOM::Element*>(children[1].get());
    ASSERT_NE(br_node, nullptr);
    EXPECT_EQ(br_node->get_tag_name(), TagNames::Br);

    auto trailing_text = dynamic_cast<Hummingbird::DOM::Text*>(children[2].get());
    ASSERT_NE(trailing_text, nullptr);
    EXPECT_EQ(trailing_text->get_text(), "World");

    auto img_node = dynamic_cast<Hummingbird::DOM::Element*>(children[3].get());
    ASSERT_NE(img_node, nullptr);
    EXPECT_EQ(img_node->get_tag_name(), TagNames::Img);
}

TEST(HtmlParserTest, TreatsMalformedTagsAsText) {
    std::string_view html = "<div><>< > </><\n></div>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    const auto& children = div_node->get_children();
    ASSERT_EQ(children.size(), 1u);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(children[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "<>< > </><\n>");
}

TEST(HtmlParserTest, TracksUnsupportedTags) {
    std::string_view html = "<custom><inner/></custom><video></video>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    const auto& unsupported = result.unsupported_tags;
    EXPECT_EQ(unsupported.size(), 3u);
    EXPECT_TRUE(unsupported.count("custom"));
    EXPECT_TRUE(unsupported.count("inner"));
    EXPECT_TRUE(unsupported.count("video"));
}

TEST(HtmlParserTest, SemanticTagsAreSupported) {
    std::string_view html = "<main><section><article></article></section></main>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    const auto& unsupported = result.unsupported_tags;
    EXPECT_FALSE(unsupported.count("main"));
    EXPECT_FALSE(unsupported.count("section"));
    EXPECT_FALSE(unsupported.count("article"));
}

namespace {
// Parses `html` and returns the first <textarea> found, so the textarea cases
// below can assert on its folded-in value.
Hummingbird::DOM::Element* first_textarea(Hummingbird::DOM::Node* node) {
    if (auto* element = dynamic_cast<Hummingbird::DOM::Element*>(node)) {
        if (element->get_tag_name() == TagNames::Textarea) {
            return element;
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = first_textarea(child.get())) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

TEST(HtmlParserTest, TextareaIsSupported) {
    std::string_view html = "<form><textarea name=\"text\"></textarea></form>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    EXPECT_FALSE(result.unsupported_tags.count("textarea"));
}

// A textarea's content is its default value, not child text. The parser folds it
// into `value` so the control has exactly one owner and nothing renders beneath
// it.
TEST(HtmlParserTest, TextareaContentBecomesItsValueRatherThanChildText) {
    std::string_view html = "<textarea name=\"text\">existing draft</textarea>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    auto* textarea = first_textarea(result.dom.get());
    ASSERT_NE(textarea, nullptr);
    const auto* value = textarea->find_attribute("value");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "existing draft");
    EXPECT_TRUE(textarea->get_children().empty());
}

TEST(HtmlParserTest, TextareaValueDecodesEntitiesAndKeepsMarkupLiteral) {
    // Escapable raw text: `<` and `&` were escaped to survive serialization, so
    // entities decode but the result is never re-tokenized as markup.
    std::string_view html = "<textarea>a &lt; b &amp;&amp; c <b>not bold</b></textarea>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    auto* textarea = first_textarea(result.dom.get());
    ASSERT_NE(textarea, nullptr);
    const auto* value = textarea->find_attribute("value");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "a < b && c <b>not bold</b>");
    EXPECT_TRUE(textarea->get_children().empty());
}

TEST(HtmlParserTest, TextareaDropsOneLeadingNewlineAfterTheStartTag) {
    // Per the HTML spec that newline is a serialization artifact; a second one is
    // real content.
    std::string_view html = "<textarea>\nkept\n</textarea>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    auto* textarea = first_textarea(result.dom.get());
    ASSERT_NE(textarea, nullptr);
    const auto* value = textarea->find_attribute("value");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "kept\n");
}

TEST(HtmlParserTest, TextareaContentDoesNotCloseTheEnclosingForm) {
    // Without RCDATA tokenization a '<' in the draft would open a bogus element
    // and drag the submit button out of the form.
    std::string_view html = "<form><textarea>3 < 4</textarea><input type=\"submit\"></form>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    auto* textarea = first_textarea(result.dom.get());
    ASSERT_NE(textarea, nullptr);
    const auto* value = textarea->find_attribute("value");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, "3 < 4");

    auto* form = dynamic_cast<Hummingbird::DOM::Element*>(textarea->get_parent());
    ASSERT_NE(form, nullptr);
    EXPECT_EQ(form->get_tag_name(), TagNames::Form);
    EXPECT_EQ(form->get_children().size(), 2u);  // textarea + submit, no stray node
}

// Raw text is literal all the way down: expanding entities here would rewrite the
// source the JS engine executes.
TEST(HtmlParserTest, ScriptBodyKeepsCharacterReferencesLiteral) {
    std::string_view html = "<script>var s = \"&amp;\";</script>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    auto* script = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(script, nullptr);
    ASSERT_EQ(script->get_children().size(), 1u);
    auto* text = dynamic_cast<Hummingbird::DOM::Text*>(script->get_children()[0].get());
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->get_text(), "var s = \"&amp;\";");
}

TEST(HtmlParserTest, CustomElementsAreSupported) {
    std::string_view html = "<my-widget><x-child></x-child></my-widget>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    const auto& unsupported = result.unsupported_tags;
    EXPECT_FALSE(unsupported.count("my-widget"));
    EXPECT_FALSE(unsupported.count("x-child"));
}

TEST(HtmlParserTest, DedupesUnsupportedTagWarnings) {
    std::string_view html = "<custom></custom><custom></custom>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    EXPECT_EQ(result.unsupported_tags.size(), 1u);
    EXPECT_TRUE(result.unsupported_tags.count("custom"));
}

TEST(HtmlParserTest, ScriptContentIsRawTextAndDoesNotLeakTags) {
    std::string_view html =
        "<html><body><script>var a = '<img src=\"http://evil/x.png\">';\n"
        "var b = '<link rel=\"stylesheet\" href=\"http://evil/x.css\">';\n"
        "if (a < b || b > a) {}</script></body></html>";
    Hummingbird::Core::ArenaAllocator arena(8192);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    // Markup-looking strings inside the script are raw text: no fake elements,
    // so nothing is discovered as an image or stylesheet resource.
    EXPECT_TRUE(result.image_links.empty());
    EXPECT_TRUE(result.stylesheet_links.empty());
    EXPECT_FALSE(result.unsupported_tags.count("img"));
    EXPECT_FALSE(result.unsupported_tags.count("link"));
}

TEST(HtmlParserTest, StyleContentIsRawText) {
    std::string_view html = "<html><head><style>a::before{content:'<b>'}</style></head></html>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.style_blocks.size(), 1u);
    // The '<b>' inside the CSS string is preserved as literal text, not a tag.
    EXPECT_NE(result.style_blocks[0].find("content:'<b>'"), std::string::npos);
}

TEST(HtmlParserTest, RawTextStopsAtMatchingEndTagCaseInsensitive) {
    // The </SCRIPT> ends raw text; the following <p> is a real element.
    std::string_view html = "<body><script>x < 1</SCRIPT><p>after</p></body>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    // Find the <p> element and confirm its text is "after".
    std::function<Hummingbird::DOM::Element*(Hummingbird::DOM::Node*)> find_p =
        [&](Hummingbird::DOM::Node* node) -> Hummingbird::DOM::Element* {
        for (const auto& child : node->get_children()) {
            if (auto* el = dynamic_cast<Hummingbird::DOM::Element*>(child.get())) {
                if (el->get_tag_name() == TagNames::P) return el;
                if (auto* found = find_p(el)) return found;
            }
        }
        return nullptr;
    };
    auto* p = find_p(result.dom.get());
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(p->get_children().size(), 1u);
    auto* text = dynamic_cast<Hummingbird::DOM::Text*>(p->get_children()[0].get());
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->get_text(), "after");
}

TEST(HtmlParserTest, DoesNotWarnForSvgChildElements) {
    std::string_view html = "<svg><rect></rect><circle/></svg>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    EXPECT_FALSE(result.unsupported_tags.count("rect"));
    EXPECT_FALSE(result.unsupported_tags.count("circle"));
}

TEST(HtmlParserTest, AutoClosesParagraphBeforeBlockTags) {
    std::string_view html = "<p>First<p>Second<dl><dt>Term</dt></dl><p>Third</p>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 4u);
    auto first = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    auto second = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[1].get());
    auto dl = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[2].get());
    auto third = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[3].get());
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(dl, nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(first->get_tag_name(), TagNames::P);
    EXPECT_EQ(second->get_tag_name(), TagNames::P);
    EXPECT_EQ(dl->get_tag_name(), TagNames::Dl);
    EXPECT_EQ(third->get_tag_name(), TagNames::P);
}

TEST(HtmlParserTest, HandlesUnclosedTags) {
    std::string_view html = "<div><span>hello";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), TagNames::Div);
    ASSERT_EQ(div_node->get_children().size(), 1u);
    auto span_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[0].get());
    ASSERT_NE(span_node, nullptr);
    EXPECT_EQ(span_node->get_tag_name(), TagNames::Span);
}

TEST(HtmlParserTest, IgnoresUnexpectedEndTags) {
    std::string_view html = "<div>hi</span><p>ok</p>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), TagNames::Div);
    ASSERT_GE(div_node->get_children().size(), 2u);
    auto paragraph_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[1].get());
    ASSERT_NE(paragraph_node, nullptr);
    EXPECT_EQ(paragraph_node->get_tag_name(), TagNames::P);
}

TEST(HtmlParserTest, DecodesNamedEntities) {
    std::string_view html = "<p>A &mdash; B &amp; C &lt; D &gt; E &quot;F&quot; &apos;G&apos; &nbsp;H</p>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    auto p_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(p_node, nullptr);
    ASSERT_EQ(p_node->get_children().size(), 1u);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(p_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    // Byte-escaped UTF-8 so the assertion is independent of the compiler's
    // execution charset (\uXXXX narrow literals are not, without /utf-8).
    const std::string expected = "A \xE2\x80\x94 B & C < D > E \"F\" 'G' \xC2\xA0H";
    EXPECT_EQ(text_node->get_text(), expected);
}

TEST(HtmlParserTest, DecodesExtendedAndNumericEntities) {
    std::string_view html = "<p>&larr; &middot; &rarr; &hellip; &#8212; &#x2192; &bogus; &#xZZ;</p>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    auto p_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(p_node, nullptr);
    ASSERT_EQ(p_node->get_children().size(), 1u);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(p_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    const std::string expected =
        "\xE2\x86\x90 \xC2\xB7 \xE2\x86\x92 \xE2\x80\xA6 \xE2\x80\x94 \xE2\x86\x92 &bogus; &#xZZ;";
    EXPECT_EQ(text_node->get_text(), expected);
}

TEST(HtmlParserTest, PopsToMatchingAncestorOnMismatchedEndTag) {
    // </div> closes both <p> and <span> scopes, then the trailing <p> should attach to root.
    std::string_view html = "<div><span><p>inner</div><p>after</p>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    ASSERT_EQ(result.dom->get_children().size(), 2u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    auto trailing_p = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[1].get());
    ASSERT_NE(div_node, nullptr);
    ASSERT_NE(trailing_p, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), TagNames::Div);

    ASSERT_EQ(div_node->get_children().size(), 1u);
    auto span_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[0].get());
    ASSERT_NE(span_node, nullptr);
    EXPECT_EQ(span_node->get_tag_name(), TagNames::Span);

    ASSERT_EQ(span_node->get_children().size(), 1u);
    auto inner_p = dynamic_cast<Hummingbird::DOM::Element*>(span_node->get_children()[0].get());
    ASSERT_NE(inner_p, nullptr);
    EXPECT_EQ(inner_p->get_tag_name(), TagNames::P);
    ASSERT_EQ(inner_p->get_children().size(), 1u);
    auto text = dynamic_cast<Hummingbird::DOM::Text*>(inner_p->get_children()[0].get());
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->get_text(), "inner");
}

TEST(HtmlParserTest, IsCaseInsensitiveForTags) {
    std::string_view html = "<DIV><A HREF='#'>Link</A></DIV>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), TagNames::Div);
    auto a_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[0].get());
    ASSERT_NE(a_node, nullptr);
    EXPECT_EQ(a_node->get_tag_name(), TagNames::A);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(a_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "Link");
}

TEST(HtmlParserTest, ExtractsStyleBlocks) {
    std::string_view html = "<style>body { color: red; }</style><p>Hi</p>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);
    const auto& styles = result.style_blocks;
    ASSERT_EQ(styles.size(), 1u);
    EXPECT_NE(styles[0].find(TagNames::Body), std::string::npos);
    EXPECT_NE(styles[0].find(PropertyNames::Color), std::string::npos);
}

TEST(HtmlParserTest, DiscoversStylesheetLinks) {
    std::string_view html =
        "<head><link rel=\"stylesheet\" href=\"site.css\"></head><body><link href='print.css' rel='StyleSheet'></body>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_EQ(result.stylesheet_links.size(), 2u);
    EXPECT_EQ(result.stylesheet_links[0], "site.css");
    EXPECT_EQ(result.stylesheet_links[1], "print.css");
}

TEST(HtmlParserTest, DiscoversImageLinks) {
    std::string_view html = "<body><img src='images/a.png'><img SRC=\"/b.jpg\"></body>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_EQ(result.image_links.size(), 2u);
    EXPECT_EQ(result.image_links[0], "images/a.png");
    EXPECT_EQ(result.image_links[1], "/b.jpg");
}

TEST(HtmlParserTest, AutoClosesListItems) {
    std::string_view html = "<ul><li>One<li>Two</ul>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto* ul = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->get_tag_name(), TagNames::Ul);
    EXPECT_EQ(ul->get_children().size(), 2u);
}

TEST(HtmlParserTest, MovesBodyOutOfHead) {
    std::string_view html = "<html><head><body><p>Hi</p></body></head></html>";
    Hummingbird::Core::ArenaAllocator arena(4096);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    ASSERT_NE(result.dom, nullptr);
    ASSERT_EQ(result.dom->get_children().size(), 1u);
    auto* html_node = dynamic_cast<Hummingbird::DOM::Element*>(result.dom->get_children()[0].get());
    ASSERT_NE(html_node, nullptr);
    ASSERT_EQ(html_node->get_tag_name(), TagNames::Html);

    ASSERT_EQ(html_node->get_children().size(), 2u);
    auto* head = dynamic_cast<Hummingbird::DOM::Element*>(html_node->get_children()[0].get());
    auto* body = dynamic_cast<Hummingbird::DOM::Element*>(html_node->get_children()[1].get());
    ASSERT_NE(head, nullptr);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(head->get_tag_name(), TagNames::Head);
    EXPECT_EQ(body->get_tag_name(), TagNames::Body);
}

// --- T-HTML-ATTR-ENTITY-DECODE-1 -------------------------------------------
namespace {
// First element with the given id, anywhere in the tree.
const Hummingbird::DOM::Element* element_by_id(const Hummingbird::DOM::Node* node, std::string_view id) {
    if (!node) return nullptr;
    if (const auto* element = dynamic_cast<const Hummingbird::DOM::Element*>(node)) {
        if (const auto* value = element->find_attribute("id"); value && *value == id) {
            return element;
        }
    }
    for (const auto& child : node->get_children()) {
        if (const auto* found = element_by_id(child.get(), id)) return found;
    }
    return nullptr;
}

std::string attribute_of(const Hummingbird::DOM::Node* root, std::string_view id, const char* name) {
    const auto* element = element_by_id(root, id);
    if (!element) return "<missing element>";
    const auto* value = element->find_attribute(name);
    return value ? *value : std::string("<missing attribute>");
}
}  // namespace

// Character references were decoded in character data but NOT in attribute
// values, so every URL containing an ampersand — which HTML *requires* be
// written `&amp;` — reached the network with the entity intact. Found by a line
// in a live browsing log:
//   http error: url=.../wiki/Sam_&amp;_Max:_Freelance_Police status=404
// on an article that exists.
TEST(HtmlParserTest, DecodesCharacterReferencesInAttributeValues) {
    std::string_view html =
        "<html><body>"
        "<a id='real' href='/wiki/Sam_&amp;_Max:_Freelance_Police'>x</a>"
        "<a id='query' href='/search?a=1&amp;b=2&amp;c=3'>y</a>"
        "<img id='img' alt='&quot;quoted&quot; &lt;tag&gt;' src='/i.png'>"
        "<a id='numeric' href='/n?x=&#38;&#x26;'>z</a>"
        "<p id='title' title='&copy;&nbsp;end'>t</p>"
        "</body></html>";
    Hummingbird::Core::ArenaAllocator arena(8192);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    // The exact failure from the log.
    EXPECT_EQ(attribute_of(result.dom.get(), "real", "href"), "/wiki/Sam_&_Max:_Freelance_Police");
    // Every separator in a query string, not just the first.
    EXPECT_EQ(attribute_of(result.dom.get(), "query", "href"), "/search?a=1&b=2&c=3");
    EXPECT_EQ(attribute_of(result.dom.get(), "img", "alt"), "\"quoted\" <tag>");
    // Numeric references, decimal and hex.
    EXPECT_EQ(attribute_of(result.dom.get(), "numeric", "href"), "/n?x=&&");
    // A named non-ASCII entity plus a non-breaking space, as UTF-8 bytes.
    // Split literal: an \x escape is greedy, so "\xA0end" would parse as one
    // out-of-range hex escape rather than U+00A0 followed by "end".
    // `&copy;` and `&nbsp;` are in the decoder's 32-name table; accented Latin
    // names such as `&eacute;` are NOT — see T-HTML-ENTITY-TABLE-1.
    EXPECT_EQ(attribute_of(result.dom.get(), "title", "title"), "\xC2\xA9\xC2\xA0" "end");
}

// The other half of the spec's rule, and the reason reusing the text decoder is
// safe here: an UNTERMINATED reference in an attribute must stay literal.
// Legacy query strings depend on it — `?a=1&amp=2` means what it says, and
// "helpfully" decoding `&amp` there would corrupt a URL that was never wrong.
TEST(HtmlParserTest, LeavesUnterminatedAndUnknownReferencesAloneInAttributes) {
    std::string_view html =
        "<html><body>"
        "<a id='bare' href='/q?a=1&amp=2&lt=3'>x</a>"
        "<a id='wordy' href='/q?x=1&ampersand=2'>y</a>"
        "<a id='unknown' href='/q?v=&notanentity;'>z</a>"
        "<a id='lone' href='/q?a=1&b=2'>w</a>"
        "</body></html>";
    Hummingbird::Core::ArenaAllocator arena(8192);
    Parser parser(arena, html);
    auto result = parser.parse();
    ASSERT_NE(result.dom, nullptr);

    EXPECT_EQ(attribute_of(result.dom.get(), "bare", "href"), "/q?a=1&amp=2&lt=3")
        << "no semicolon, so there is nothing to decode";
    EXPECT_EQ(attribute_of(result.dom.get(), "wordy", "href"), "/q?x=1&ampersand=2");
    EXPECT_EQ(attribute_of(result.dom.get(), "unknown", "href"), "/q?v=&notanentity;")
        << "an unknown name is preserved verbatim";
    EXPECT_EQ(attribute_of(result.dom.get(), "lone", "href"), "/q?a=1&b=2")
        << "a bare ampersand is already correct and must not change";
}
