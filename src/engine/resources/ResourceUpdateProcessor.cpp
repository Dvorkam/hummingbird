#include "engine/resources/ResourceUpdateProcessor.h"

#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "engine/resources/ResourceRequestPlanner.h"

namespace Hummingbird::Engine::ResourceUpdateProcessor {

namespace {
void handle_document_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store,
                            ResourceLoader::BatchResult& result, ProcessingStats& stats) {
    store.mark_ready(update.url, update.type, std::move(update.body));
    stats.mark_ready(ResourceType::Document);
    result.document_url = update.url;
    result.effective_url = update.effective_url;
    result.document_error = update.error;
}

bool handle_image_decode(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store,
                         IImageDecoder* image_decoder, ProcessingStats& stats) {
    if (!image_decoder) {
        HB_LOG_WARN("[image] decode skipped (no decoder): " << update.url);
        store.mark_failed(update.url, update.type);
        return false;
    }
    const auto decode_start = Core::Clock::now();
    auto animated = image_decoder->decode_animation(update.body);
    if (animated) {
        store.mark_ready(update.url, update.type, std::move(update.body));
        store.set_animation(update.url, update.type, std::move(*animated));
        const auto decode_end = Core::Clock::now();
        stats.image_decode_ms += Core::duration_ms(decode_start, decode_end);
        ++stats.image_decode_count;
        return true;
    }
    auto decoded = image_decoder->decode(update.body);
    const auto decode_end = Core::Clock::now();
    stats.image_decode_ms += Core::duration_ms(decode_start, decode_end);
    ++stats.image_decode_count;
    if (!decoded) {
        HB_LOG_WARN("[image] decode failed: " << update.url);
        store.mark_failed(update.url, update.type);
        return false;
    }
    store.mark_ready(update.url, update.type, std::move(update.body));
    store.set_image(update.url, update.type, std::move(*decoded));
    return true;
}

void handle_failed_update(const ResourceLoader::PendingResourceUpdate& update, ResourceStore& store) {
    store.mark_failed(update.url, update.type);
    if (update.type == ResourceType::Document) {
        HB_LOG_WARN("[resource] document failed to load: " << update.url);
    }
}
}  // namespace

void process_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store, IImageDecoder* image_decoder,
                    ResourceLoader::BatchResult& result, ProcessingStats& stats) {
    if (!update.success) {
        handle_failed_update(update, store);
        // A failed DOCUMENT still carries a navigation result the tab needs:
        // which URL failed, where the chain ended up, and why. Dropping it here
        // is what made a redirect loop indistinguishable from a blank page, and
        // would have left story 8.3.2's error pages with nothing to render.
        // `ready` is deliberately not set — the document did not load.
        if (update.type == ResourceType::Document) {
            result.document_url = update.url;
            result.effective_url = update.effective_url;
            result.document_error = update.error;
        }
        return;
    }

    // Document stays hand-handled: it carries navigation results (url,
    // effective url, error) that no other resource type has.
    if (update.type == ResourceType::Document) {
        handle_document_update(update, store, result, stats);
        return;
    }

    const auto& options = ResourceRequestPlanning::request_options_for(update.type);
    bool ready = false;
    switch (options.decode) {
        case ResourceRequestPlanning::ResourceDecode::Image:
            ready = handle_image_decode(update, store, image_decoder, stats);
            break;
        case ResourceRequestPlanning::ResourceDecode::None:
            store.mark_ready(update.url, update.type, std::move(update.body));
            ready = true;
            break;
    }
    if (ready) {
        stats.mark_ready(update.type);
    }
}

}  // namespace Hummingbird::Engine::ResourceUpdateProcessor
