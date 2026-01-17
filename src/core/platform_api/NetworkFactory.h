#pragma once

#include "core/platform_api/INetwork.h"

namespace Hummingbird {

enum class NetworkBackend {
    Curl,
    Stub,
};

NetworkPtr create_network(NetworkBackend backend);

}  // namespace Hummingbird
