#pragma once

#include <iostream>

// Define HB_ENABLE_LOG to enable info/warn logs; HB_ENABLE_DEBUG_LOG adds debug logs.
#ifdef HB_ENABLE_LOG
#define HB_LOG_INFO(msg) (std::cerr << "[info] " << msg << std::endl)
#define HB_LOG_WARN(msg) (std::cerr << "[warn] " << msg << std::endl)
#define HB_LOG_ERROR(msg) (std::cerr << "[error] " << msg << std::endl)
#ifdef HB_ENABLE_DEBUG_LOG
#define HB_LOG_DEBUG(msg) (std::cerr << "[debug] " << msg << std::endl)
#else
#define HB_LOG_DEBUG(msg) ((void)0)
#endif
#else
#define HB_LOG_INFO(msg) ((void)0)
#define HB_LOG_WARN(msg) ((void)0)
#define HB_LOG_ERROR(msg) (std::cerr << "[error] " << msg << std::endl)
#define HB_LOG_DEBUG(msg) ((void)0)
#endif
