#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/dom/Node.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::DOM {

class Element : public Node {
public:
    enum class PseudoState {
        Hover,
        Active,
        Focus,
        Visited,
    };

    struct AttributeHash {
        using is_transparent = void;

        size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }
        size_t operator()(const std::string& value) const noexcept { return std::hash<std::string_view>{}(value); }
    };

    struct AttributeEq {
        using is_transparent = void;

        bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
        bool operator()(const std::string& lhs, const std::string& rhs) const noexcept { return lhs == rhs; }
        bool operator()(const std::string& lhs, std::string_view rhs) const noexcept { return lhs == rhs; }
        bool operator()(std::string_view lhs, const std::string& rhs) const noexcept { return lhs == rhs; }
    };

    using AttributeMap = std::unordered_map<std::string, std::string, AttributeHash, AttributeEq>;

    static Core::ArenaPtr<Element> create(Core::ArenaAllocator& arena, std::string_view tag_name) {
        return Core::ArenaPtr<Element>(Core::arena_new<Element>(arena, tag_name));
    }

    const std::string& get_tag_name() const { return m_tag_name; }
    const AttributeMap& get_attributes() const { return m_attributes; }

    const std::string* find_attribute(std::string_view key) const {
        auto it = m_attributes.find(key);
        if (it == m_attributes.end()) return nullptr;
        return &it->second;
    }

    void set_attribute(std::string_view key, std::string_view value) {
        m_attributes[Core::Utils::to_lower(key)] = std::string(value);
    }

    bool has_attribute(std::string_view key) const { return m_attributes.find(key) != m_attributes.end(); }

    void remove_attribute(std::string_view key) { m_attributes.erase(Core::Utils::to_lower(key)); }

    // --- class attribute helpers (classList, 7.1.2) ---
    // Tokens are space-separated and case-sensitive, matching how the selector
    // matcher reads the class attribute (SelectorMatcher::has_all_classes).
    bool class_contains(std::string_view token) const {
        if (token.empty()) return false;
        const auto* value = find_attribute("class");
        if (!value) return false;
        for (auto existing : Core::Utils::split_ascii_whitespace(*value)) {
            if (existing == token) return true;
        }
        return false;
    }

    // Adds a token if absent; returns true when the class list changed.
    bool class_add(std::string_view token) {
        if (token.empty() || class_contains(token)) return false;
        auto& value = m_attributes[std::string("class")];
        if (!value.empty()) value.push_back(' ');
        value.append(token);
        return true;
    }

    // Removes every occurrence of a token; returns true when the list changed.
    bool class_remove(std::string_view token) {
        if (token.empty()) return false;
        const auto* value = find_attribute("class");
        if (!value) return false;
        std::string rebuilt;
        bool removed = false;
        for (auto existing : Core::Utils::split_ascii_whitespace(*value)) {
            if (existing == token) {
                removed = true;
                continue;
            }
            if (!rebuilt.empty()) rebuilt.push_back(' ');
            rebuilt.append(existing);
        }
        if (!removed) return false;
        m_attributes[std::string("class")] = std::move(rebuilt);
        return true;
    }

    std::optional<std::string_view> get_accessibility_role() const {
        if (const auto* explicit_role = find_attribute("role"); explicit_role && !explicit_role->empty()) {
            return std::string_view(*explicit_role);
        }
        return implied_accessibility_role();
    }

    bool has_pseudo_state(PseudoState state) const {
        switch (state) {
            case PseudoState::Hover:
                return pseudo_hover_;
            case PseudoState::Active:
                return pseudo_active_;
            case PseudoState::Focus:
                return pseudo_focus_;
            case PseudoState::Visited:
                return pseudo_visited_;
        }
        return false;
    }

    bool set_pseudo_state(PseudoState state, bool enabled) {
        bool* slot = nullptr;
        switch (state) {
            case PseudoState::Hover:
                slot = &pseudo_hover_;
                break;
            case PseudoState::Active:
                slot = &pseudo_active_;
                break;
            case PseudoState::Focus:
                slot = &pseudo_focus_;
                break;
            case PseudoState::Visited:
                slot = &pseudo_visited_;
                break;
        }
        if (!slot || *slot == enabled) {
            return false;
        }
        *slot = enabled;
        return true;
    }

private:
    bool has_accessible_name() const {
        const auto is_non_empty = [this](std::string_view name) {
            const auto* value = find_attribute(name);
            return value && !Core::Utils::trim_ascii_whitespace(*value).empty();
        };
        return is_non_empty("aria-label") || is_non_empty("aria-labelledby") || is_non_empty("title");
    }

    std::optional<std::string_view> implied_accessibility_role() const {
        if (m_tag_name == "header") return std::string_view("banner");
        if (m_tag_name == "nav") return std::string_view("navigation");
        if (m_tag_name == "main") return std::string_view("main");
        if (m_tag_name == "section" && has_accessible_name()) return std::string_view("region");
        if (m_tag_name == "article") return std::string_view("article");
        if (m_tag_name == "aside") return std::string_view("complementary");
        if (m_tag_name == "footer") return std::string_view("contentinfo");
        return std::nullopt;
    }

    template <typename T, typename... Args>
    // Allow arena_new to invoke the private constructor while keeping creation centralized.
    friend T* Core::arena_new(Core::ArenaAllocator&, Args&&...);

    explicit Element(std::string_view tag_name) : m_tag_name(tag_name) {}

    std::string m_tag_name;
    std::unordered_map<std::string, std::string, AttributeHash, AttributeEq> m_attributes;
    bool pseudo_hover_ = false;
    bool pseudo_active_ = false;
    bool pseudo_focus_ = false;
    bool pseudo_visited_ = false;
};

}  // namespace Hummingbird::DOM
