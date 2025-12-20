#pragma once

#include "core/INetwork.h"
#include <string>

// A simple network stub that returns canned HTML for known URLs.
class StubNetwork : public INetwork {
public:
    void get(const std::string& url, std::function<void(std::string)> callback) override;
};
