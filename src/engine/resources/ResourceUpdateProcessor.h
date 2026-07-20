#pragma once

#include <stddef.h>

#include <array>

#include "core/platform_api/IImageDecoder.h"
#include "engine/resources/ResourceLoader.h"

namespace Hummingbird::Engine::ResourceUpdateProcessor {

struct ProcessingStats {
    std::array<bool, kResourceTypeCount> ready{};
    size_t image_decode_count = 0;
    double image_decode_ms = 0.0;

    bool is_ready(ResourceType type) const { return ready[static_cast<size_t>(type)]; }
    void mark_ready(ResourceType type) { ready[static_cast<size_t>(type)] = true; }
};

void process_update(ResourceLoader::PendingResourceUpdate& update, ResourceStore& store, IImageDecoder* image_decoder,
                    ResourceLoader::BatchResult& result, ProcessingStats& stats);

}  // namespace Hummingbird::Engine::ResourceUpdateProcessor
