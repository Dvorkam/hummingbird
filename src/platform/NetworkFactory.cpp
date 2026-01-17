#include "core/platform_api/NetworkFactory.h"

#include "platform/CurlNetwork.h"
#include "platform/StubNetwork.h"

namespace Hummingbird {

NetworkPtr create_network(NetworkBackend backend) {
    switch (backend) {
        case NetworkBackend::Curl:
            return std::make_unique<Hummingbird::Platform::CurlNetwork>();
        case NetworkBackend::Stub:
            return std::make_unique<Hummingbird::Platform::StubNetwork>();
    }
    return nullptr;
}

}  // namespace Hummingbird
