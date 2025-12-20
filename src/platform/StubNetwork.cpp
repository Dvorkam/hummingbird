#include "platform/StubNetwork.h"
#include <thread>

void StubNetwork::get(const std::string& url, std::function<void(std::string)> callback) {
    // Deliver on a short background task to mimic asynchrony.
    std::thread([url, cb = std::move(callback)]() mutable {
        std::string body;
        if (url == "http://example.com" || url == "https://example.com") {
            body = "<html><body><p>Example Domain</p><p>This domain is for use in illustrative examples.</p></body></html>";
        } else {
            body = "<html><body><p>Loaded: " + url + "</p></body></html>";
        }
        cb(std::move(body));
    }).detach();
}
