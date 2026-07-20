#pragma once

#include <string>
#include <vector>

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Engine {

// One <script> element in document order: external (src set, inline body
// ignored per spec) or inline (text set). async/defer collapse to document
// order for now (7.0.1 MVP).
struct DocumentScriptRef {
    std::string src;
    std::string text;

    bool is_external() const { return !src.empty(); }
};

std::vector<DocumentScriptRef> collect_document_scripts_from_dom(const DOM::Node* root);
std::vector<std::string> collect_background_image_links_from_dom(const DOM::Node* root);

}  // namespace Hummingbird::Engine
