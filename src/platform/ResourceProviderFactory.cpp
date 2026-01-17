#include "core/platform_api/ResourceProviderFactory.h"

#include "platform/FileResourceProvider.h"

namespace Hummingbird {

ResourceProviderPtr create_resource_provider() {
    return std::make_unique<Hummingbird::Platform::FileResourceProvider>();
}

}  // namespace Hummingbird
