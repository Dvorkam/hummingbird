#pragma once

#include "engine/document/DocumentPainter.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
}

namespace Hummingbird::Engine {

class DocumentInteraction;
class DocumentModel;

class DocumentRenderer {
public:
    struct PaintContext {
        Layout::Rect viewport;
        bool debug_outlines = false;
        float scroll_y = 0.0f;
    };

    DocumentRenderer(DocumentModel& model, DocumentInteraction& interaction);

    void reset();

    void relayout(IGraphicsContext& graphics, const Layout::Rect& viewport);
    void paint(IGraphicsContext& graphics, const PaintContext& context);
    void paint_controls(IGraphicsContext& graphics, const PaintContext& context, bool repaint_background);

    float content_height() const { return content_height_; }

private:
    DocumentModel& model_;
    DocumentInteraction& interaction_;
    DocumentPainter painter_;
    float content_height_ = 0.0f;
};

}  // namespace Hummingbird::Engine
