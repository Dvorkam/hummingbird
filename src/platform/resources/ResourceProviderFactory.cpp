#include "core/platform_api/ResourceProviderFactory.h"

#include <memory>

#include "platform/resources/FileResourceProvider.h"

namespace Hummingbird {

ResourceProviderPtr create_resource_provider() {
    return std::make_unique<Hummingbird::Platform::FileResourceProvider>();
}

}  // namespace Hummingbird
