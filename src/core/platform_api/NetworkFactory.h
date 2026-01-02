#pragma once

#include "core/platform_api/INetwork.h"

enum class NetworkBackend {
    Curl,
    Stub,
};

NetworkPtr create_network(NetworkBackend backend);
