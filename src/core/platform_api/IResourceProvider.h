#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

class IResourceProvider {
public:
    virtual ~IResourceProvider() = default;

    // Load a text resource by id/path. Returns nullopt if unavailable.
    virtual std::optional<std::string> load_text(std::string_view resource_id) = 0;

    // Load a raw/binary resource by id/path. Returns nullopt if unavailable.
    // Default implementation defers to load_text for providers that don't differentiate.
    virtual std::optional<std::string> load_bytes(std::string_view resource_id) {
        return load_text(resource_id);
    }
};

using ResourceProviderPtr = std::unique_ptr<IResourceProvider>;
