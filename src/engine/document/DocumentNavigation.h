#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "engine/forms/FormSubmission.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentModel;

class DocumentNavigation final {
public:
    explicit DocumentNavigation(const DocumentModel& model) : model_(model) {}

    std::optional<std::string> hit_test_link(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                             const Layout::Rect& viewport, float scroll_y,
                                             std::string_view base_url) const;
    std::optional<FormSubmission> submit_form_at(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                                 const Layout::Rect& viewport, float scroll_y,
                                                 std::string_view base_url) const;

private:
    const DocumentModel& model_;
};

}  // namespace Hummingbird::Engine
