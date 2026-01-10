#pragma once

#include <functional>
#include <string>

#include "core/platform_api/INetwork.h"

inline NetworkResponse make_response(const std::string& url) {
    NetworkResponse response;
    response.url = url;
    return response;
}

inline NetworkResponse make_response_with_effective_url(const std::string& url) {
    NetworkResponse response;
    response.url = url;
    response.effective_url = url;
    return response;
}

inline bool respond_if_stopping(bool stopping, std::function<void(NetworkResponse)>& callback, const std::string& url) {
    if (!stopping) return false;
    if (callback) callback(make_response(url));
    return true;
}
