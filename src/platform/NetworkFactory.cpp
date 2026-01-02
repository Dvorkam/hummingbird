#include "core/platform_api/NetworkFactory.h"

#include "platform/CurlNetwork.h"
#include "platform/StubNetwork.h"

NetworkPtr create_network(NetworkBackend backend) {
    switch (backend) {
        case NetworkBackend::Curl:
            return std::make_unique<CurlNetwork>();
        case NetworkBackend::Stub:
            return std::make_unique<StubNetwork>();
    }
    return nullptr;
}
