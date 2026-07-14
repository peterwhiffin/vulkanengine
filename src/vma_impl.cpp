#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION

#define VMA_LEAK_LOG_FORMAT(format, ...)                         \
	do {                                                     \
		printf("VMA Leak: " format "\n", ##__VA_ARGS__); \
	} while (0)

#include "vk_mem_alloc.h"
