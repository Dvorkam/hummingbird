#pragma once

#include "core/platform_api/IResourceProvider.h"

namespace Hummingbird::Platform {

class FileResourceProvider : public IResourceProvider {
public:
    std::optional<std::string> load_text(std::string_view resource_id) override;
    std::optional<std::string> load_bytes(std::string_view resource_id) override;
};

}  // namespace Hummingbird::Platform
