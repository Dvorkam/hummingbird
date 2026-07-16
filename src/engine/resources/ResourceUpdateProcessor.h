#pragma once

#include <stddef.h>

#include "core/platform_api/IImageDecoder.h"
#include "engine/resources/ResourceLoader.h"

namespace Hummingbird::Engine::ResourceUpdateProcessor {

struct ProcessingStats {
    bool document_ready = false;
    bool stylesheet_ready = false;
    bool image_ready = false;
    bool font_ready = false;
    size_t image_decode_count = 0;
    double image_decode_ms = 0.0;
};

void process_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store, IImageDecoder* image_decoder,
                    ResourceLoader::BatchResult& result, ProcessingStats& stats);

}  // namespace Hummingbird::Engine::ResourceUpdateProcessor
