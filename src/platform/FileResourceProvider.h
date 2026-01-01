#pragma once

#include "core/platform_api/IResourceProvider.h"

class FileResourceProvider : public IResourceProvider {
public:
    std::optional<std::string> load_text(std::string_view resource_id) override;
};
