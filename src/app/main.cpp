#include <memory>
#include <string>
#include <utility>

#include "app/BrowserApp.h"
#include "core/platform_api/IWindow.h"
#include "core/platform_api/WindowFactory.h"
#include "core/utils/Log.h"

int main(int /*argc*/, char* /*argv*/[]) {
    // Tee this session's log to a timestamped file before anything else runs.
    // Console scrollback has a fixed size, so a long browsing session — exactly
    // the kind worth logging — loses its own beginning. Started here rather than
    // inside a library so test binaries never leave log files behind.
    const std::string log_path = Hummingbird::Core::Log::start_file_logging();
    if (!log_path.empty()) {
        HB_LOG_INFO("[log] session log: " << log_path);
    }

    auto window = Hummingbird::create_window();
    window->open();

    if (!window->is_open()) {
        Hummingbird::Core::Log::stop_file_logging();
        return 1;
    }

    Hummingbird::App::BrowserApp app(std::move(window));
    app.start();  // initial navigation + initial UI focus

    while (app.tick()) {  // one “frame”
        // nothing here
    }

    Hummingbird::Core::Log::stop_file_logging();
    return 0;
}
