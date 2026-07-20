#include "stdio.h"
#include "vulkan/vulkan.h"

#define NO_LOG 0
#define LOG_LVL_ERR 1
#define LOG_LVL_WARN 2
#define LOG_LVL_INFO 3

#ifndef LOG_LVL
#define LOG_LVL NO_LOG
#endif

#if LOG_LVL > NO_LOG
#define LOG_ERR(msg, ...) printf("[ERROR] " msg "\n", __VA_ARGS__);
#define LOG_VK(msg, ...) printf(msg "\n", __VA_ARGS__);
#else
#define LOG_ERR(msg, ...)
#endif

#if LOG_LVL > LOG_LVL_ERR
#define LOG_WARN(msg, ...) printf("[WARNING] " msg "\n", __VA_ARGS__);
#else
#define LOG_WARN(msg, ...)
#endif

#if LOG_LVL > LOG_LVL_WARN
#define LOG_INFO(msg, ...) printf("[INFO] " msg "\n", __VA_ARGS__);
#else
#define LOG_INFO(msg, ...)
#endif
