#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"

namespace Hummingbird::Test {

class InlineNetwork final : public INetwork {
public:
    InlineNetwork(std::string body, std::string effective_url = {})
        : body_(std::move(body)), effective_url_(std::move(effective_url)) {}

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        (void)options;
        NetworkResponse response;
        response.url = url;
        response.effective_url = effective_url_.empty() ? url : effective_url_;
        response.status = 200;
        response.body = body_;
        if (callback) callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        (void)body;
        get(url, std::move(callback), options);
    }

    void shutdown() override {}

private:
    std::string body_;
    std::string effective_url_;
};

class RoutingNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }

    // Response headers the fake server returns for `url` (Set-Cookie, ...).
    void set_response_headers(const std::string& url, Core::HttpHeaders headers) {
        response_headers_[url] = std::move(headers);
    }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        requested_.push_back(url);
        sent_headers_.push_back(options.headers);
        auto it = responses_.find(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != responses_.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (auto headers = response_headers_.find(url); headers != response_headers_.end()) {
            response.headers = headers->second;
        }
        if (callback) callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        (void)body;
        get(url, std::move(callback), options);
    }

    void shutdown() override {}

    const std::vector<std::string>& requested() const { return requested_; }
    // Request headers supplied for each call, parallel to requested().
    const std::vector<Core::HttpHeaders>& sent_headers() const { return sent_headers_; }

private:
    std::unordered_map<std::string, std::string> responses_;
    std::unordered_map<std::string, Core::HttpHeaders> response_headers_;
    std::vector<std::string> requested_;
    std::vector<Core::HttpHeaders> sent_headers_;
};

class DeferredNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }
    void defer_response(const std::string& url, std::string body) { deferred_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        (void)options;
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

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        (void)body;
        get(url, std::move(callback), options);
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

}  // namespace Hummingbird::Test
