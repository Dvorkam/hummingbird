#pragma once

#include <iostream>

#ifndef HB_LOG_LEVEL
#define HB_LOG_LEVEL 1  // ERROR
#endif

// HB_LOG_LEVEL: 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#if HB_LOG_LEVEL >= 3
#define HB_LOG_INFO(msg) (std::cerr << "[info] " << msg << std::endl)
#else
#define HB_LOG_INFO(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 2
#define HB_LOG_WARN(msg) (std::cerr << "[warn] " << msg << std::endl)
#else
#define HB_LOG_WARN(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 1
#define HB_LOG_ERROR(msg) (std::cerr << "[error] " << msg << std::endl)
#else
#define HB_LOG_ERROR(msg) ((void)0)
#endif

#if HB_LOG_LEVEL >= 4
#define HB_LOG_DEBUG(msg) (std::cerr << "[debug] " << msg << std::endl)
#else
#define HB_LOG_DEBUG(msg) ((void)0)
#endif
