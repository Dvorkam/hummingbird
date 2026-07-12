#pragma once

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird {
class IWindow;
}

namespace Hummingbird::App {

class BrowserChrome;
class TabController;

class RenderCoordinator {
public:
    RenderCoordinator(IWindow* window, IGraphicsContext* graphics, BrowserChrome& chrome, TabController& tabs)
        : window_(window), graphics_(graphics), chrome_(chrome), tabs_(tabs) {}

    void set_document_dirty() { document_dirty_ = true; }
    void set_chrome_dirty() { chrome_dirty_ = true; }
    void set_controls_dirty() { controls_dirty_ = true; }
    void set_document_and_controls_dirty() {
        document_dirty_ = true;
        controls_dirty_ = true;
    }
    void set_all_dirty() {
        chrome_dirty_ = true;
        set_document_and_controls_dirty();
    }

    void invalidate_document_cache() {
        document_cache_valid_ = false;
        document_dirty_ = true;
    }

    bool document_dirty() const { return document_dirty_; }
    bool chrome_dirty() const { return chrome_dirty_; }
    bool controls_dirty() const { return controls_dirty_; }

    void toggle_debug_outlines() { debug_outlines_ = !debug_outlines_; }
    bool debug_outlines() const { return debug_outlines_; }

    void render_if_needed();

private:
    IWindow* window_ = nullptr;
    IGraphicsContext* graphics_ = nullptr;
    BrowserChrome& chrome_;
    TabController& tabs_;
    bool document_dirty_ = true;
    bool chrome_dirty_ = true;
    bool controls_dirty_ = false;
    bool document_cache_valid_ = false;
    bool debug_outlines_ = false;
};

}  // namespace Hummingbird::App
