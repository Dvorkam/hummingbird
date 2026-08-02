#include "platform/net/CurlNetwork.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "platform/net/NetworkThreadPool.h"

TEST(CurlNetworkTest, AcceptEncodingIsEmptyForAutoDecompression) {
    EXPECT_STREQ(Hummingbird::Platform::CurlNetwork::accept_encoding(), "");
}

// Closing the browser must not wait for the network.
//
// `shutdown()` JOINS its workers, which is correct — nothing may still be
// touching engine state afterwards. The bug was that a worker parked inside
// curl had no way to learn it should stop, so the join lasted until the request
// hit its own deadline. Live symptom: a page with nine webfonts on an
// unreachable host hung the window on close, then reported every request at
// once as the deadlines expired together.
//
// This is the property the curl progress callback restores, expressed without
// curl: a task that polls `stopping()` is released promptly, and the flag is
// observable from inside a running task rather than only between tasks.
TEST(NetworkThreadPoolTest, ShutdownReleasesATaskThatIsStillRunning) {
    Hummingbird::Platform::NetworkThreadPool pool;

    std::atomic<bool> started{false};
    std::atomic<bool> observed_stop{false};

    // Stands in for a transfer: it runs until told to stop, exactly as the curl
    // progress callback now polls the same flag.
    const auto* stopping = pool.stopping_flag();
    pool.submit([&started, &observed_stop, stopping] {
        started.store(true);
        while (!stopping->load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        observed_stop.store(true);
    });

    while (!started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto begin = std::chrono::steady_clock::now();
    pool.shutdown();
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    EXPECT_TRUE(observed_stop.load()) << "the running task must be able to see the stop flag";
    // Generous by two orders of magnitude against the 15s request budget that
    // used to bound this, so the assertion means "promptly" rather than "fast".
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 2000)
        << "shutdown waited for the task instead of releasing it";
}

TEST(NetworkThreadPoolTest, TheStopFlagIsSetBeforeWorkersAreJoined) {
    Hummingbird::Platform::NetworkThreadPool pool;
    EXPECT_FALSE(pool.stopping());
    EXPECT_FALSE(pool.stopping_flag()->load());

    pool.shutdown();

    // Ordering is the whole mechanism: if the flag were set after the join, a
    // running transfer could never observe it and the join would hang first.
    EXPECT_TRUE(pool.stopping());
    EXPECT_TRUE(pool.stopping_flag()->load());
}
