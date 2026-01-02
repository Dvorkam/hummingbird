#include "core/platform_api/ResourceProviderFactory.h"

#include "platform/FileResourceProvider.h"

ResourceProviderPtr create_resource_provider() {
    return std::make_unique<FileResourceProvider>();
}
