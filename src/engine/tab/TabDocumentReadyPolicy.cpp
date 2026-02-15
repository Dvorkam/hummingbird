#include "engine/tab/TabDocumentReadyPolicy.h"

#include "core/utils/Log.h"
#include "core/utils/Url.h"
#include "engine/document/DocumentPipeline.h"
#include "engine/resources/ResourceLoader.h"

namespace Hummingbird::Engine {

namespace {
SecurityState security_state_for_url(std::string_view url) {
    auto parsed = Core::parse_absolute_url(url);
    if (!parsed) return SecurityState::Unknown;
    if (parsed->scheme == "https") return SecurityState::Secure;
    if (parsed->scheme == "http") return SecurityState::InsecureHttp;
    return SecurityState::Unknown;
}
}  // namespace

SecurityState TabDocumentReadyPolicy::decide_security_state(const ResourceLoader& loader,
                                                            std::string_view requested_url,
                                                            NetworkError document_error) {
    if (document_error == NetworkError::TlsVerificationFailed) {
        return SecurityState::InsecureTls;
    }
    if (loader.is_insecure_allowed_for_url(requested_url)) {
        return SecurityState::InsecureTls;
    }
    return security_state_for_url(requested_url);
}

void TabDocumentReadyPolicy::log_discovered_resources(DocumentPipeline::LayoutApi layout) {
    if (!layout.stylesheet_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered stylesheet links: " << layout.stylesheet_links().size());
    }
    if (!layout.image_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered image sources: " << layout.image_links().size());
    }
    if (!layout.background_image_links().empty()) {
        HB_LOG_INFO("[pipeline] discovered background images: " << layout.background_image_links().size());
    }
}

void TabDocumentReadyPolicy::request_discovered_resources(ResourceLoader& loader, DocumentPipeline::LayoutApi layout,
                                                          std::string_view base_url) {
    loader.request_stylesheets(layout.stylesheet_links(), base_url);
    loader.request_images(layout.image_links(), base_url);
}

}  // namespace Hummingbird::Engine
