#include "core/platform_api/NetworkFactory.h"

#include <gtest/gtest.h>

TEST(NetworkFactoryTest, CreatesBackends) {
    auto curl = Hummingbird::create_network(Hummingbird::NetworkBackend::Curl);
    ASSERT_NE(curl, nullptr);
    curl->shutdown();

    auto stub = Hummingbird::create_network(Hummingbird::NetworkBackend::Stub);
    ASSERT_NE(stub, nullptr);
    stub->shutdown();
}
