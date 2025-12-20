#pragma once

#include <string>

#include "core/INetwork.h"

// A simple network stub that returns canned HTML for known URLs.
class StubNetwork : public INetwork {
public:
    void get(const std::string& url, std::function<void(std::string)> callback) override;
};
