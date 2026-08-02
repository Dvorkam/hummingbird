#include "engine/extensions/FilterRuleSet.h"

#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace Hummingbird::Engine {
namespace {

// A cursor over the ruleset text.
//
// This is a second hand-rolled JSON reader in this directory — ExtensionManifest
// has the first. That duplication is deliberate for now (the established pattern
// is a small parser per module, and refactoring mid-feature would widen the
// diff), but it is real: see T-JSON-SHARED-PARSER-1.
struct Cursor {
    std::string_view text;
    std::size_t offset = 0;

    bool eof() const { return offset >= text.size(); }
    char peek() const { return eof() ? '\0' : text[offset]; }
};

void skip_ws(Cursor& c) {
    while (!c.eof() && std::isspace(static_cast<unsigned char>(c.peek()))) ++c.offset;
}

bool match(Cursor& c, char expected) {
    skip_ws(c);
    if (c.eof() || c.text[c.offset] != expected) return false;
    ++c.offset;
    return true;
}

std::optional<std::string> parse_string(Cursor& c) {
    skip_ws(c);
    if (c.eof() || c.text[c.offset] != '"') return std::nullopt;
    ++c.offset;
    std::string out;
    while (!c.eof()) {
        const char ch = c.text[c.offset++];
        if (ch == '"') return out;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (c.eof()) break;
        const char escaped = c.text[c.offset++];
        switch (escaped) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            // Everything else stands for itself, which covers \" \\ and \/.
            // Deliberately no \uXXXX: a filter rule matching a URL has no use
            // for one, and a half-implemented unicode escape is worse than an
            // absent one.
            default:
                out.push_back(escaped);
                break;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> parse_uint(Cursor& c) {
    skip_ws(c);
    const std::size_t start = c.offset;
    while (!c.eof() && std::isdigit(static_cast<unsigned char>(c.peek()))) ++c.offset;
    if (c.offset == start) return std::nullopt;
    return static_cast<std::uint32_t>(
        std::strtoul(std::string(c.text.substr(start, c.offset - start)).c_str(), nullptr, 10));
}

bool skip_value(Cursor& c);

bool skip_container(Cursor& c, char open, char close) {
    if (!match(c, open)) return false;
    skip_ws(c);
    if (match(c, close)) return true;
    while (true) {
        if (!skip_value(c)) return false;
        skip_ws(c);
        if (match(c, close)) return true;
        if (match(c, ',')) continue;
        // An object's "key": value pair reaches here after the key.
        if (match(c, ':')) continue;
        return false;
    }
}

bool skip_value(Cursor& c) {
    skip_ws(c);
    if (c.eof()) return false;
    const char ch = c.peek();
    if (ch == '"') return parse_string(c).has_value();
    if (ch == '{') return skip_container(c, '{', '}');
    if (ch == '[') return skip_container(c, '[', ']');
    while (!c.eof()) {
        const char next = c.peek();
        if (next == ',' || next == '}' || next == ']') break;
        ++c.offset;
    }
    return true;
}

std::optional<std::vector<std::string>> parse_string_array(Cursor& c) {
    if (!match(c, '[')) return std::nullopt;
    std::vector<std::string> out;
    skip_ws(c);
    if (match(c, ']')) return out;
    while (true) {
        auto item = parse_string(c);
        if (!item) return std::nullopt;
        out.push_back(std::move(*item));
        skip_ws(c);
        if (match(c, ']')) return out;
        if (!match(c, ',')) return std::nullopt;
    }
}

struct RuleDraft {
    Core::FilterRule rule;
    bool has_block_action = false;
    std::string error;

    // Records a reason to skip this rule WITHOUT abandoning the parse.
    //
    // The distinction matters and got this wrong once: bailing out the moment a
    // field is invalid leaves the cursor in the middle of an object, so the
    // outer parser resumes at a position that is not a rule boundary and either
    // fails the whole file or misreads the next rule. So the parse always runs
    // to the end of the object; only the verdict changes. First reason wins, so
    // the message names the first thing actually wrong.
    void reject(std::string why) {
        if (error.empty()) error = std::move(why);
    }
};

// `condition` object.
bool parse_condition(Cursor& c, RuleDraft& draft) {
    if (!match(c, '{')) return false;
    skip_ws(c);
    if (match(c, '}')) return true;
    while (true) {
        auto key = parse_string(c);
        if (!key) return false;
        if (!match(c, ':')) return false;

        if (*key == "urlFilter") {
            auto value = parse_string(c);
            if (!value) return false;
            draft.rule.url_filter = std::move(*value);
        } else if (*key == "requestDomains") {
            auto domains = parse_string_array(c);
            if (!domains) return false;
            // One domain per rule in M9. A list is accepted because MV3's field
            // is a list, but taking only the first would silently drop the rest
            // — so say so instead.
            if (domains->size() > 1) {
                draft.reject("requestDomains takes one domain in this engine; split the rule");
            } else if (!domains->empty()) {
                draft.rule.request_domain = std::move(domains->front());
            }
        } else if (*key == "resourceTypes") {
            auto names = parse_string_array(c);
            if (!names) return false;
            for (const auto& name : *names) {
                Core::RequestDestination destination{};
                if (!Core::parse_request_destination(name, &destination)) {
                    draft.reject("unknown resourceType \"" + name + "\"");
                    continue;
                }
                draft.rule.destinations.push_back(destination);
            }
        } else if (*key == "domainType") {
            auto value = parse_string(c);
            if (!value) return false;
            if (*value == "thirdParty") {
                draft.rule.third_party = Core::ThirdPartyScope::ThirdPartyOnly;
            } else if (*value == "firstParty") {
                draft.rule.third_party = Core::ThirdPartyScope::FirstPartyOnly;
            } else {
                draft.reject("domainType must be \"thirdParty\" or \"firstParty\"");
            }
        } else {
            if (!skip_value(c)) return false;
        }

        skip_ws(c);
        if (match(c, '}')) return true;
        if (!match(c, ',')) return false;
    }
}

// `action` object. Only `{"type": "block"}` is accepted.
bool parse_action(Cursor& c, RuleDraft& draft) {
    if (!match(c, '{')) return false;
    skip_ws(c);
    if (match(c, '}')) return true;
    while (true) {
        auto key = parse_string(c);
        if (!key) return false;
        if (!match(c, ':')) return false;
        if (*key == "type") {
            auto value = parse_string(c);
            if (!value) return false;
            if (*value == "block") {
                draft.has_block_action = true;
            } else {
                draft.reject("unsupported action type \"" + *value + "\"; only \"block\" is implemented");
            }
        } else {
            if (!skip_value(c)) return false;
        }
        skip_ws(c);
        if (match(c, '}')) return true;
        if (!match(c, ',')) return false;
    }
}

bool parse_rule(Cursor& c, RuleDraft& draft) {
    if (!match(c, '{')) return false;
    skip_ws(c);
    if (match(c, '}')) return true;
    while (true) {
        auto key = parse_string(c);
        if (!key) return false;
        if (!match(c, ':')) return false;

        if (*key == "id") {
            auto id = parse_uint(c);
            if (!id) return false;
            draft.rule.id = *id;
        } else if (*key == "condition") {
            if (!parse_condition(c, draft)) return false;
        } else if (*key == "action") {
            if (!parse_action(c, draft)) return false;
        } else {
            if (!skip_value(c)) return false;
        }

        skip_ws(c);
        if (match(c, '}')) return true;
        if (!match(c, ',')) return false;
    }
}

}  // namespace

RuleSetParseResult parse_filter_rule_set(std::string_view json) {
    RuleSetParseResult result;
    Cursor c{json, 0};

    if (!match(c, '[')) {
        result.fatal_error = "a ruleset must be a JSON array of rules";
        return result;
    }
    skip_ws(c);
    if (match(c, ']')) return result;

    std::size_t index = 0;
    while (true) {
        RuleDraft draft;
        if (!parse_rule(c, draft)) {
            result.fatal_error = "malformed rule at index " + std::to_string(index);
            result.rules.clear();
            return result;
        }

        const auto reject = [&](std::string why) {
            result.warnings.push_back("rule " + std::to_string(index) + " skipped: " + std::move(why));
        };

        if (!draft.error.empty()) {
            reject(std::move(draft.error));
        } else if (!draft.has_block_action) {
            reject("no action; every rule must declare {\"type\": \"block\"}");
        } else if (!draft.rule.constrains_something()) {
            // The dangerous one. A rule with neither a domain nor a substring
            // matches every URL, so accepting it would take the whole web off
            // the air on behalf of an author who almost certainly made a typo.
            reject("matches everything; a rule needs a urlFilter or a requestDomain");
        } else {
            result.rules.push_back(std::move(draft.rule));
        }

        ++index;
        skip_ws(c);
        if (match(c, ']')) break;
        if (!match(c, ',')) {
            result.fatal_error = "expected ',' or ']' after rule " + std::to_string(index - 1);
            result.rules.clear();
            return result;
        }
    }
    return result;
}

}  // namespace Hummingbird::Engine
