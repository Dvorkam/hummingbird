#pragma once

#include <atomic>

#include "core/INetwork.h"

class CurlNetwork : public INetwork {
public:
    CurlNetwork();
    ~CurlNetwork() override;

    void get(const std::string& url, std::function<void(std::string)> callback) override;

    bool ok() const { return m_initialized; }

private:
    std::atomic<bool> m_initialized{false};
};
