#include "html/HtmlEntityDecoder.h"

namespace Hummingbird::Html::Utils {

std::string decode_named_entities(std::string_view text) {
    if (text.find('&') == std::string_view::npos) {
        return std::string(text);
    }
    std::string decoded;
    decoded.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '&') {
            decoded.push_back(text[i]);
            ++i;
            continue;
        }
        size_t semi = text.find(';', i + 1);
        if (semi == std::string_view::npos) {
            decoded.push_back(text[i]);
            ++i;
            continue;
        }
        std::string_view name = text.substr(i + 1, semi - i - 1);
        if (name == "amp") {
            decoded.push_back('&');
        } else if (name == "lt") {
            decoded.push_back('<');
        } else if (name == "gt") {
            decoded.push_back('>');
        } else if (name == "quot") {
            decoded.push_back('"');
        } else if (name == "apos") {
            decoded.push_back('\'');
        } else if (name == "nbsp") {
            decoded.append("\u00A0");
        } else if (name == "mdash") {
            decoded.append("\u2014");
        } else {
            decoded.append(text.substr(i, semi - i + 1));
        }
        i = semi + 1;
    }
    return decoded;
}

}  // namespace Hummingbird::Html::Utils
