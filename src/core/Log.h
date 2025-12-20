#pragma once

#include <iostream>

// Simple opt-in logging macros. Define HB_ENABLE_LOG at compile time to enable.
#ifdef HB_ENABLE_LOG
    #define HB_LOG_INFO(msg)  (std::cerr << "[info] " << msg << std::endl)
    #define HB_LOG_WARN(msg)  (std::cerr << "[warn] " << msg << std::endl)
    #define HB_LOG_ERROR(msg) (std::cerr << "[error] " << msg << std::endl)
#else
    #define HB_LOG_INFO(msg)  ((void)0)
    #define HB_LOG_WARN(msg)  ((void)0)
    #define HB_LOG_ERROR(msg) ((void)0)
#endif

