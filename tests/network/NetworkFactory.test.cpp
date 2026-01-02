#include <gtest/gtest.h>

#include "core/platform_api/NetworkFactory.h"

TEST(NetworkFactoryTest, CreatesBackends) {
    auto curl = create_network(NetworkBackend::Curl);
    ASSERT_NE(curl, nullptr);
    curl->shutdown();

    auto stub = create_network(NetworkBackend::Stub);
    ASSERT_NE(stub, nullptr);
    stub->shutdown();
}
