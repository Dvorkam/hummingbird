#pragma once

#include "core/platform_api/IGraphicsContext.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::App {

class BrowserApp;

class RenderCoordinator {
public:
    explicit RenderCoordinator(BrowserApp& app) : app_(app) {}

    void set_document_dirty() { document_dirty_ = true; }
    void set_chrome_dirty() { chrome_dirty_ = true; }
    void set_controls_dirty() { controls_dirty_ = true; }

    void invalidate_document_cache() {
        document_cache_valid_ = false;
        document_dirty_ = true;
    }

    bool document_dirty() const { return document_dirty_; }
    bool chrome_dirty() const { return chrome_dirty_; }
    bool controls_dirty() const { return controls_dirty_; }

    void render_if_needed();

private:
    BrowserApp& app_;
    bool document_dirty_ = true;
    bool chrome_dirty_ = true;
    bool controls_dirty_ = false;
    bool document_cache_valid_ = false;
};

}  // namespace Hummingbird::App
