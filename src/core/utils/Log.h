#pragma once

#include <sstream>
#include <string>

namespace Hummingbird::Core::Log {

// Writes one already-formatted line to every active sink. `level` is the
// bracketed tag ("[warn] ").
//
// The whole line is emitted under one lock, which is why the macros below build
// it into a string first rather than streaming straight at the sink: the
// network thread pool logs from its own threads, and a chain of `<<` calls
// interleaves mid-line when two of them race.
void emit(const char* level, const std::string& message);

// Starts teeing output to a timestamped file and returns its path (empty on
// failure — logging then continues to the console alone, because losing the log
// must never be a reason the browser does not start).
//
// Opt-in, and called only by the application: a library that opened a file just
// because it was linked would litter one per test binary. Overrides, in order:
//   HB_LOG_FILE   exact path to write
//   HB_LOG_DIR    directory to write into (default "logs")
// The default location is relative to the working directory; it should move
// with the rest of the profile data under T-PROFILE-DATA-DIR-1.
std::string start_file_logging();

// Closes the file sink. Console output continues.
void stop_file_logging();

}  // namespace Hummingbird::Core::Log

#ifndef HB_LOG_LEVEL
#define HB_LOG_LEVEL 1  // ERROR
#endif

// Builds the message, then hands the finished line to the sink. The
// do/while(0) makes the macro a statement, so it is safe after a bare `if`.
#define HB_LOG_EMIT(level, msg)                                      \
    do {                                                             \
        std::ostringstream hb_log_stream_;                           \
        hb_log_stream_ << msg;                                       \
        ::Hummingbird::Core::Log::emit(level, hb_log_stream_.str()); \
    } while (0)

// HB_LOG_LEVEL: 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#if HB_LOG_LEVEL >= 3
#define HB_LOG_INFO(msg) HB_LOG_EMIT("[info] ", msg)
#else
#define HB_LOG_INFO(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 2
#define HB_LOG_WARN(msg) HB_LOG_EMIT("[warn] ", msg)
#else
#define HB_LOG_WARN(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 1
#define HB_LOG_ERROR(msg) HB_LOG_EMIT("[error] ", msg)
#else
#define HB_LOG_ERROR(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 4
#define HB_LOG_DEBUG(msg) HB_LOG_EMIT("[debug] ", msg)
#else
#define HB_LOG_DEBUG(msg) ((void)0)
#endif
