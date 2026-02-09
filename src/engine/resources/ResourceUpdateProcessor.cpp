#include "engine/resources/ResourceUpdateProcessor.h"

#include "core/utils/Log.h"
#include "core/utils/Timing.h"

namespace Hummingbird::Engine::ResourceUpdateProcessor {

namespace {
void handle_document_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store,
                            ResourceLoader::BatchResult& result, ProcessingStats& stats) {
    store.mark_ready(update.url, update.type, std::move(update.body));
    stats.document_ready = true;
    result.document_url = update.url;
    result.effective_url = update.effective_url;
    result.document_error = update.error;
}

void handle_stylesheet_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store,
                              ProcessingStats& stats) {
    store.mark_ready(update.url, update.type, std::move(update.body));
    stats.stylesheet_ready = true;
}

void handle_image_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store,
                         IImageDecoder* image_decoder, ProcessingStats& stats) {
    if (!image_decoder) {
        HB_LOG_WARN("[image] decode skipped (no decoder): " << update.url);
        store.mark_failed(update.url, update.type);
        return;
    }
    const auto decode_start = Core::Clock::now();
    auto animated = image_decoder->decode_animation(update.body);
    if (animated) {
        store.mark_ready(update.url, update.type, std::move(update.body));
        store.set_animation(update.url, update.type, std::move(*animated));
        const auto decode_end = Core::Clock::now();
        stats.image_decode_ms += Core::duration_ms(decode_start, decode_end);
        ++stats.image_decode_count;
        stats.image_ready = true;
        return;
    }
    auto decoded = image_decoder->decode(update.body);
    const auto decode_end = Core::Clock::now();
    stats.image_decode_ms += Core::duration_ms(decode_start, decode_end);
    ++stats.image_decode_count;
    if (!decoded) {
        HB_LOG_WARN("[image] decode failed: " << update.url);
        store.mark_failed(update.url, update.type);
        return;
    }
    store.mark_ready(update.url, update.type, std::move(update.body));
    store.set_image(update.url, update.type, std::move(*decoded));
    stats.image_ready = true;
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
        return;
    }

    if (update.type == ResourceType::Document) {
        handle_document_update(update, store, result, stats);
    } else if (update.type == ResourceType::Stylesheet) {
        handle_stylesheet_update(update, store, stats);
    } else if (update.type == ResourceType::Image) {
        handle_image_update(update, store, image_decoder, stats);
    }
}

}  // namespace Hummingbird::Engine::ResourceUpdateProcessor
