#pragma once

#include <string_view>

#include "core/SecurityState.h"
#include "core/platform_api/INetwork.h"

namespace Hummingbird::Engine {

class DocumentPipeline;
class ResourceLoader;
class TabNavigationState;

class TabDocumentReadyPolicy {
public:
    static SecurityState decide_security_state(const ResourceLoader& loader, std::string_view requested_url,
                                               NetworkError document_error);

    static void log_discovered_resources(const DocumentPipeline& pipeline);

    static void request_discovered_resources(ResourceLoader& loader, const DocumentPipeline& pipeline,
                                             std::string_view base_url);
};

}  // namespace Hummingbird::Engine
