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

    std::optional<std::string_view> get_accessibility_role() const {
        if (const auto* explicit_role = find_attribute("role"); explicit_role && !explicit_role->empty()) {
            return std::string_view(*explicit_role);
        }
        return implied_accessibility_role();
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
};

}  // namespace Hummingbird::DOM
