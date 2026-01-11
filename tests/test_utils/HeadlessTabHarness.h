#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "engine/Tab.h"
#include "test_utils/TestGraphicsContext.h"

namespace Hummingbird::Test {

class InlineNetwork final : public INetwork {
public:
    InlineNetwork(std::string body, std::string effective_url = {})
        : body_(std::move(body)), effective_url_(std::move(effective_url)) {}

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        NetworkResponse response;
        response.url = url;
        response.effective_url = effective_url_.empty() ? url : effective_url_;
        response.status = 200;
        response.body = body_;
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

private:
    std::string body_;
    std::string effective_url_;
};

class RoutingNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        requested_.push_back(url);
        auto it = responses_.find(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != responses_.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

    const std::vector<std::string>& requested() const { return requested_; }

private:
    std::unordered_map<std::string, std::string> responses_;
    std::vector<std::string> requested_;
};

class DeferredNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }
    void defer_response(const std::string& url, std::string body) { deferred_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        requested_.push_back(url);
        if (deferred_.find(url) != deferred_.end()) {
            pending_[url] = std::move(callback);
            return;
        }
        auto it = responses_.find(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != responses_.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (callback) callback(std::move(response));
    }

    void complete(const std::string& url) {
        auto pending_it = pending_.find(url);
        if (pending_it == pending_.end()) return;
        auto deferred_it = deferred_.find(url);
        std::string body = deferred_it == deferred_.end() ? std::string{} : deferred_it->second;
        auto callback = std::move(pending_it->second);
        pending_.erase(pending_it);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (!body.empty()) {
            response.status = 200;
            response.body = std::move(body);
        }
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

private:
    std::unordered_map<std::string, std::string> responses_;
    std::unordered_map<std::string, std::string> deferred_;
    std::unordered_map<std::string, std::function<void(NetworkResponse)>> pending_;
    std::vector<std::string> requested_;
};

class InlineImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override {
        if (bytes.empty()) {
            return std::nullopt;
        }
        ImageBitmap bitmap;
        bitmap.width = 2;
        bitmap.height = 2;
        bitmap.stride = 8;
        bitmap.format = PixelFormat::PRGB32;
        bitmap.pixels.assign(static_cast<size_t>(bitmap.stride) * bitmap.height, 0xFF);
        return bitmap;
    }
};

class HeadlessTabHarness {
public:
    HeadlessTabHarness(NetworkPtr network, NetworkPtr fallback, ResourceProviderPtr provider,
                       ImageDecoderPtr decoder = nullptr)
        : tab_(std::move(network), std::move(fallback), std::move(provider), std::move(decoder)) {}

    void set_viewport(const Layout::Rect& viewport) { viewport_ = viewport; }
    const Layout::Rect& viewport() const { return viewport_; }

    void navigate(std::string_view url) { tab_.navigate(url); }
    bool tick() { return tab_.tick(context_, viewport_); }
    void paint(bool debug_outlines = false) { tab_.paint(context_, viewport_, debug_outlines); }

    std::optional<Engine::ResourceView> resource_view(std::string_view url, Engine::ResourceType type) const {
        return tab_.resource_view(url, type);
    }

    Engine::Tab& tab() { return tab_; }
    const Engine::Tab& tab() const { return tab_; }

private:
    TestGraphicsContext context_;
    Layout::Rect viewport_{0, 0, 800, 600};
    Engine::Tab tab_;
};

}  // namespace Hummingbird::Test
