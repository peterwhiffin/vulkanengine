#include "stdlib.h"
#include "assert.h"

#include "SDL3/SDL_vulkan.h"
#include "SDL3/SDL_init.h"

#define VOLK_IMPLEMENTATION
#define VK_NO_PROTOTYPES
#include "volk.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "log.h"
#include "types.h"

bool const engine_validation_layers_enabled = true;
u32 const FIF = 2;

// clang-format off
float vertices[] = {
	-1.0f,-1.0f, 0.0f, 0.0f, 
	-1.0f, 1.0f, 0.0f, 1.0f,
	 1.0f,-1.0f, 1.0f, 0.0f,
	 1.0f, 1.0f, 1.0f, 1.0f
};

// float vertices[] = {
// 	0.5f,0.5f, 0.0f, 0.0f, 
// 	 0.0f, -0.5f, 0.0f, 1.0f,
// 	 -0.5f,0.5f, 1.0f, 0.0f,
// };
// clang-format on

u32 indices[] = { 0, 1, 2, 3 };
// u32 indices[] = { 0, 1, 2 };

void vk_chk(VkResult result, char *msg)
{
	if (result != VK_SUCCESS) {
		LOG_ERR("VK::%s: FAILED! (%u)", msg, result);
		exit(result);
	} else if (msg) {
		LOG_INFO("VK::%s", msg);
	}
}

void sdl_chk(bool result, char *msg)
{
	if (!result) {
		LOG_ERR("SDL::%s: Failed! (%u)", msg, result);
		exit(result);
	} else if (msg) {
		LOG_INFO("SDL::%s", msg);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
						    VkDebugUtilsMessageTypeFlagsEXT type,
						    VkDebugUtilsMessengerCallbackDataEXT const *pCallbackData,
						    void *pUserData)
{
	// std::cerr << "validation layer: type " << type << " msg: " << pCallbackData->pMessage << std::endl;
	printf("validation layer: type %u msg: %s\n", type, pCallbackData->pMessage);
	return VK_FALSE;
}

char *read_file(char *file, size_t *size)
{
	size_t len;
	char *dst;
	FILE *f = fopen(file, "rb");

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	dst = malloc(len + 1);
	fread(dst, sizeof(char), len, f);
	dst[len] = '\0';
	fclose(f);

	*size = len;
	return dst;
}

void vk_init_debug_messenger(struct render_state *ren)
{
	VkDebugUtilsMessageSeverityFlagsEXT severityFlags = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
							    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	VkDebugUtilsMessageTypeFlagsEXT messageTypeFlags = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = severityFlags,
		.messageType = messageTypeFlags,
		.pfnUserCallback = debugCallback
	};
	// debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	vkCreateDebugUtilsMessengerEXT(ren->instance, &debugUtilsMessengerCreateInfoEXT, nullptr,
				       &ren->debug_messenger);
}

VkCommandBuffer vk_begin_one_time_cmds(struct render_state *ren)
{
	VkCommandBuffer cmd;

	VkCommandBufferAllocateInfo ainfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = ren->cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};

	vkAllocateCommandBuffers(ren->device, &ainfo, &cmd);
	VkCommandBufferBeginInfo cbi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(cmd, &cbi);
	return cmd;
}

void vk_end_one_time_cmds(struct render_state *ren, VkCommandBuffer cmd)
{
	vk_chk(vkEndCommandBuffer(cmd), "Ending Command Buffer");
	VkSubmitInfo sinfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
	};

	//may want a fence here instead of waitidle
	vkQueueSubmit(ren->gfx_q, 1, &sinfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(ren->gfx_q);
	vkFreeCommandBuffers(ren->device, ren->cmd_pool, 1, &cmd);
}

void vk_create_instance(struct render_state *ren)
{
	u32 sdl_ext_count = 0;
	u32 lay_count = 1;
	u32 ext_count = 1;

	char const *ext[16];
	char const *layers = "VK_LAYER_KHRONOS_validation";
	char const *my_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	char const *const *sdl_ext = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);

	ext_count += sdl_ext_count;

	memcpy(ext, sdl_ext, sizeof(char *) * sdl_ext_count);
	memcpy(ext + sdl_ext_count, &my_ext, sizeof(char *));

	for (u32 i = 0; i < ext_count; i++) {
		LOG_INFO("VK::Extension enabled: %s", ext[i]);
	}

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.pApplicationName = "Vulkan Engine",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "None",
		.engineVersion = VK_MAKE_VERSION(0, 0, 0),
		.apiVersion = VK_API_VERSION_1_4,
	};

	VkInstanceCreateInfo instanceInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = lay_count,
		.ppEnabledLayerNames = &layers,
		.enabledExtensionCount = ext_count,
		.ppEnabledExtensionNames = ext,
	};

	vk_chk(vkCreateInstance(&instanceInfo, NULL, &ren->instance), "Creating Instance");
	volkLoadInstance(ren->instance);
}

void vk_select_physical_device(struct render_state *ren)
{
	u32 i = 0;
	u32 count = 0;
	VkPhysicalDevice devices[8];

	vk_chk(vkEnumeratePhysicalDevices(ren->instance, &count, NULL), "Checking Physical Device Count");
	assert(count > 0);

	vk_chk(vkEnumeratePhysicalDevices(ren->instance, &count, devices), "Getting Physical Devices");

	//TODO: Actually find the best device
	ren->physical_device = devices[i];

	VkPhysicalDeviceProperties2 props = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };

	vkGetPhysicalDeviceProperties2(ren->physical_device, &props);
	LOG_INFO("VK::Device Found: %s", props.properties.deviceName);
}

void vk_create_device(struct render_state *ren)
{
	u32 fam_count = 0;
	float qp = 1.0f;
	VkQueueFamilyProperties fams[32];
	char const *const ext[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	vkGetPhysicalDeviceQueueFamilyProperties(ren->physical_device, &fam_count, NULL);
	vkGetPhysicalDeviceQueueFamilyProperties(ren->physical_device, &fam_count, fams);

	for (u32 i = 0; i < fam_count; i++) {
		if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			sdl_chk(SDL_Vulkan_GetPresentationSupport(ren->instance, ren->physical_device, i),
				"Get Presentation Support");
			ren->gfx_q_fam = i;
		}
	}

	VkDeviceQueueCreateInfo qci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueFamilyIndex = ren->gfx_q_fam,
		.queueCount = 1,
		.pQueuePriorities = &qp,
	};

	VkPhysicalDeviceFeatures f10 = {
		.samplerAnisotropy = true,
	};

	VkPhysicalDeviceVulkan12Features f12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.descriptorIndexing = true,
		.shaderSampledImageArrayNonUniformIndexing = true,
		.descriptorBindingVariableDescriptorCount = true,
		.runtimeDescriptorArray = true,
		.bufferDeviceAddress = true,
	};

	VkPhysicalDeviceVulkan13Features f13 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &f12,
		.synchronization2 = true,
		.dynamicRendering = true,
	};

	VkDeviceCreateInfo dci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &f13,
		.flags = 0,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &qci,
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = ext,
		.pEnabledFeatures = &f10,
	};

	vk_chk(vkCreateDevice(ren->physical_device, &dci, NULL, &ren->device), "Creating Device");
	vkGetDeviceQueue(ren->device, ren->gfx_q_fam, 0, &ren->gfx_q);
}

void vk_init_vma(struct render_state *ren)
{
	VmaVulkanFunctions f = {
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkCreateImage = vkCreateImage,
	};

	VmaAllocatorCreateInfo aci = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = ren->physical_device,
		.device = ren->device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = NULL,
		.pDeviceMemoryCallbacks = NULL,
		.pHeapSizeLimit = NULL,
		.pVulkanFunctions = &f,
		.instance = ren->instance,
		.vulkanApiVersion = VK_API_VERSION_1_4,
	};

	vk_chk(vmaCreateAllocator(&aci, &ren->allocator), "Creating VMA Allocator");
}

void sdl_create_window(struct render_state *ren, struct window *win)
{
	win->w = 800;
	win->h = 600;
	// SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
	sdl_chk(win->p_win = SDL_CreateWindow("Vulkan Engine", win->w, win->h, flags), "Creating Window");
	sdl_chk(SDL_Vulkan_CreateSurface(win->p_win, ren->instance, NULL, &win->surf), "Creating Vulkan Surface");
	sdl_chk(SDL_GetWindowSize(win->p_win, (int *)&win->w, (int *)&win->h), "Getting Window Size");
}

void vk_create_swapchain(struct render_state *ren, struct window *win)
{
	VkSurfaceCapabilitiesKHR caps;
	VkExtent2D ext;
	VkImage *i_tmp;
	u32 count = 0;

	vk_chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ren->physical_device, win->surf, &caps),
	       "Getting Surface Capabilities");

	if (caps.currentExtent.width == 0xFFFFFFFF) {
		ext.width = win->w;
		ext.height = win->h;
	} else {
		ext = caps.currentExtent;
	}

	ren->swap_fmt = VK_FORMAT_B8G8R8A8_SRGB;

	VkSwapchainCreateInfoKHR sci = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = NULL,
		.flags = 0,
		.surface = win->surf,
		.minImageCount = caps.minImageCount,
		.imageFormat = ren->swap_fmt,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = ext,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = 0,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.clipped = 0,
		.oldSwapchain = NULL,
	};

	vk_chk(vkCreateSwapchainKHR(ren->device, &sci, NULL, &ren->swapchain), "Creating Swapchain");
	vk_chk(vkGetSwapchainImagesKHR(ren->device, ren->swapchain, &count, NULL), "Checking Swapchain Image Count");

	ren->swap_count = count;
	ren->swap_images = malloc(sizeof(*ren->swap_images) * count);
	i_tmp = malloc(sizeof(VkImage) * count);

	vk_chk(vkGetSwapchainImagesKHR(ren->device, ren->swapchain, &count, i_tmp), "Getting Swapchain images");

	for (u32 i = 0; i < count; i++) {
		ren->swap_images[i].image = i_tmp[i];

		VkImageSubresourceRange sr = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};

		VkImageViewCreateInfo vci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.image = ren->swap_images[i].image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_B8G8R8A8_SRGB,
			.components = { 0 },
			.subresourceRange = sr,
		};

		vk_chk(vkCreateImageView(ren->device, &vci, NULL, &ren->swap_images[i].view),
		       "Creating Swapchain View");
	}

	free(i_tmp);
}

void vk_create_depth_buffer(struct render_state *ren, struct window *win)
{
	VkFormat fmts[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	ren->depth_fmt = VK_FORMAT_UNDEFINED;
	VkFormatProperties2 props = { .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };

	for (int i = 0; i < 2; i++) {
		vkGetPhysicalDeviceFormatProperties2(ren->physical_device, fmts[i], &props);
		if (props.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			ren->depth_fmt = fmts[i];
			break;
		}
	}

	VkExtent3D ext = {
		.width = win->w,
		.height = win->h,
		.depth = 1,
	};

	VkImageCreateInfo dci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = ren->depth_fmt,
		.extent = ext,
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo aci = {
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	vk_chk(vmaCreateImage(ren->allocator, &dci, &aci, &ren->depth_image.image, &ren->depth_image.alloc, NULL),
	       "Creating Depth Image");
	vmaSetAllocationName(ren->allocator, ren->depth_image.alloc, "depth image allocation");

	VkImageSubresourceRange sr = {
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};

	VkImageViewCreateInfo vci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = ren->depth_image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = ren->depth_fmt,
		.components = { 0 },
		.subresourceRange = sr,
	};

	vk_chk(vkCreateImageView(ren->device, &vci, NULL, &ren->depth_image.view), "Creating Depth View");
}

void vk_create_uniform_buffers(struct render_state *ren)
{
	ren->ubuf = malloc(sizeof(*ren->ubuf) * FIF);

	for (u32 i = 0; i < FIF; i++) {
		VkBufferCreateInfo bci = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.size = sizeof(struct uniform_data),
			.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
		};

		VmaAllocationCreateInfo aci = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};

		vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &ren->ubuf[i].buf, &ren->ubuf[i].alloc,
				       &ren->ubuf[i].info),
		       "Allocating Uniform Buffer");
		vmaSetAllocationName(ren->allocator, ren->ubuf[i].alloc, "Uniform Buffer");

		VkBufferDeviceAddressInfo dai = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = NULL,
			.buffer = ren->ubuf[i].buf,
		};

		ren->ubuf[i].addr = vkGetBufferDeviceAddress(ren->device, &dai);
	}

	//TODO: need a generic create buffer function eventually
	ren->test_uniform_buf = malloc(sizeof(*ren->test_uniform_buf) * FIF);

	for (u32 i = 0; i < FIF; i++) {
		VkBufferCreateInfo bci = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.size = sizeof(struct uniform_test),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
		};

		VmaAllocationCreateInfo aci = {
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};

		vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &ren->test_uniform_buf[i].buf,
				       &ren->test_uniform_buf[i].alloc, &ren->test_uniform_buf[i].info),
		       "Allocating Test Uniform Buffer");
		vmaSetAllocationName(ren->allocator, ren->test_uniform_buf[i].alloc, "Test Uniform Buffer");

		// VkBufferDeviceAddressInfo dai = {
		// 	.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		// 	.pNext = NULL,
		// 	.buffer = ren->ubuf[i].buf,
		// };

		ren->test_uniform_buf[i].addr = 0;
	}
}

void vk_create_sync_objects(struct render_state *ren)
{
	ren->fences = malloc(sizeof(*ren->fences) * FIF);
	ren->img_sems = malloc(sizeof(*ren->img_sems) * FIF);
	ren->ren_sems = malloc(sizeof(*ren->ren_sems) * ren->swap_count);

	VkSemaphoreCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
	};

	VkFenceCreateInfo fci = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = NULL,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (u32 i = 0; i < FIF; i++) {
		vk_chk(vkCreateFence(ren->device, &fci, NULL, &ren->fences[i]), "Creating Fence");
		vk_chk(vkCreateSemaphore(ren->device, &sci, NULL, &ren->img_sems[i]),
		       "Creating Image Acquired Semaphore");
	}

	for (u32 i = 0; i < ren->swap_count; i++) {
		vk_chk(vkCreateSemaphore(ren->device, &sci, NULL, &ren->ren_sems[i]),
		       "Creating Render Complete Semaphore");
	}
}

void vk_allocate_command_buffers(struct render_state *ren)
{
	ren->cmds = malloc(sizeof(*ren->cmds) * FIF);

	VkCommandPoolCreateInfo cci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = ren->gfx_q_fam,
	};

	vk_chk(vkCreateCommandPool(ren->device, &cci, NULL, &ren->cmd_pool), "Creating Command Pool");

	VkCommandBufferAllocateInfo cba = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = NULL,
		.commandPool = ren->cmd_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = FIF,
	};

	vk_chk(vkAllocateCommandBuffers(ren->device, &cba, ren->cmds), "Allocating Command Buffers");
}

void vk_load_texture(struct render_state *ren)
{
	int w, h, ch;
	char *file = "../../res/texture.jpg";
	stbi_uc *d = stbi_load(file, &w, &h, &ch, STBI_rgb_alpha);
	VkDeviceSize size = w * h * STBI_rgb_alpha;

	if (!d) {
		LOG_ERR("STB::Failed to load image: %s", file);
		exit(1);
	}

	//take largest dim, log2 returns the number of times it can be divided by 2,
	//and take the floor for dims that aren't powers of 2.
	u32 mip_levels = (u32)floorf(log2f(fmaxf(w, h))) + 1;
	VkBuffer staging_buff;
	VmaAllocation staging_alloc;
	VkBufferCreateInfo sbci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};
	VmaAllocationCreateInfo saci = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	VmaAllocationInfo sinfo;
	vk_chk(vmaCreateBuffer(ren->allocator, &sbci, &saci, &staging_buff, &staging_alloc, &sinfo),
	       "Allocating Texture Staging Buffer");
	vmaSetAllocationName(ren->allocator, staging_alloc, "Texture Staging Buffer");
	memcpy(sinfo.pMappedData, d, size);
	stbi_image_free(d);

	VkExtent3D ext = {
		.width = w,
		.height = h,
		.depth = 1,
	};

	VkImageCreateInfo ici = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.extent = ext,
		.mipLevels = mip_levels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo iaci = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	vk_chk(vmaCreateImage(ren->allocator, &ici, &iaci, &ren->tex_image.image, &ren->tex_image.alloc, NULL),
	       "Allocating Texture Image");
	vmaSetAllocationName(ren->allocator, ren->tex_image.alloc, "Texture Image");

	VkImageSubresourceRange sub = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = mip_levels,
		.layerCount = 1,
	};

	VkImageViewCreateInfo vci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = ren->tex_image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = ici.format,
		.subresourceRange = sub,
	};

	vk_chk(vkCreateImageView(ren->device, &vci, NULL, &ren->tex_image.view), "Creating Image View");
	VkCommandBuffer cmd = vk_begin_one_time_cmds(ren);

	VkImageSubresourceRange rng = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = mip_levels,
		.layerCount = 1,
	};

	VkImageMemoryBarrier2 bar = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = ren->tex_image.image,
		.subresourceRange = rng,
	};

	VkDependencyInfo dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &bar,
	};

	vkCmdPipelineBarrier2(cmd, &dep);

	VkImageSubresourceLayers subs = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.layerCount = 1,
		.baseArrayLayer = 0,
	};

	VkBufferImageCopy cpy = {
		.bufferOffset = 0,
		.bufferImageHeight = 0,
		.imageSubresource = subs,
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { w, h, 1 },
	};
	vkCmdCopyBufferToImage(cmd, staging_buff, ren->tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);
	vk_end_one_time_cmds(ren, cmd);
	vmaDestroyBuffer(ren->allocator, staging_buff, staging_alloc);
}

void vk_load_model(struct render_state *ren)
{
	VkDeviceSize v_size = sizeof(vertices);
	VkDeviceSize i_size = sizeof(indices);

	VkBufferCreateInfo bci = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = v_size + i_size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	};

	VmaAllocationCreateInfo aci = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
			 VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	VmaAllocationInfo info;

	vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &ren->vert_buff, &ren->vert_alloc, &info),
	       "Allocating Vertex Buffer");
	vmaSetAllocationName(ren->allocator, ren->vert_alloc, "Vertex Buffer");

	memcpy(info.pMappedData, vertices, v_size);
	memcpy(((char *)info.pMappedData) + v_size, indices, i_size);
}

void vk_load_shaders(struct render_state *ren)
{
	size_t s = 0;
	char *c = read_file("shader.spv", &s);
	VkShaderModuleCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = s,
		.pCode = (u32 *)c,
	};

	vk_chk(vkCreateShaderModule(ren->device, &sci, NULL, &ren->shader), "Creating Shader Module");
}

void vk_create_descriptor_pool(struct render_state *ren)
{
	VkDescriptorPoolSize pool_sizes[2];

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 1000;

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = 1000;

	VkDescriptorPoolCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = 2,
		.pPoolSizes = pool_sizes,
	};

	vk_chk(vkCreateDescriptorPool(ren->device, &pci, NULL, &ren->desc_pool), "Creating Descriptor Pool");
}

void vk_create_descriptor_sets(struct render_state *ren)
{
	u32 num_sets = FIF;
	VkDescriptorSetLayout *layouts = malloc(sizeof(*layouts) * num_sets);
	ren->set_test = malloc(sizeof(*ren->set_test) * num_sets);

	for (u32 i = 0; i < num_sets; i++) {
		layouts[i] = ren->set_layout_buf;
	}

	VkDescriptorSetAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ren->desc_pool,
		.descriptorSetCount = num_sets,
		.pSetLayouts = layouts,
	};

	vk_chk(vkAllocateDescriptorSets(ren->device, &ai, ren->set_test), "Allocating Descriptor Sets");

	for (u32 i = 0; i < FIF; i++) {
		VkDescriptorBufferInfo binfo = {
			.buffer = ren->test_uniform_buf[i].buf,
			.offset = 0,
			.range = sizeof(struct uniform_test),
		};
		VkWriteDescriptorSet writes[1] = { 0 };

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = ren->set_test[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &binfo;

		vkUpdateDescriptorSets(ren->device, 1, writes, 0, NULL);
	}

	free(layouts);
}

void vk_create_descriptor_set_layout(struct render_state *ren)
{
	VkDescriptorSetLayoutBinding bindings[1];

	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = bindings,
	};

	vkCreateDescriptorSetLayout(ren->device, &dci, NULL, &ren->set_layout_buf);
}

void vk_create_pipeline(struct render_state *ren)
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.size = sizeof(VkDeviceAddress),
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &ren->set_layout_buf, //TODO: texture set layout
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange,
	};

	vk_chk(vkCreatePipelineLayout(ren->device, &pipelineLayoutCI, nullptr, &ren->pipeline_layout),
	       "Creating Pipeline Layout");

	VkVertexInputBindingDescription vertexBinding = {
		.binding = 0,
		.stride = sizeof(float) * 4,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertexAttributes[2] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 2 },
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBinding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertexAttributes,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
	};

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_VERTEX_BIT,
		  .module = ren->shader,
		  .pName = "vertMain" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		  .module = ren->shader,
		  .pName = "fragMain" },
	};

	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
	};

	VkPipelineRenderingCreateInfo renderingCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &ren->swap_fmt,
		.depthAttachmentFormat = ren->depth_fmt,
	};

	VkPipelineColorBlendAttachmentState blendAttachment = { .colorWriteMask = 0xF };

	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	VkGraphicsPipelineCreateInfo pipelineCI = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingCI,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = ren->pipeline_layout,
	};

	vk_chk(vkCreateGraphicsPipelines(ren->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &ren->pipeline),
	       "Creating Pipeline");
}

void vk_init(struct render_state *ren, struct window *win)
{
	ren->frame_index = 0;
	sdl_chk(SDL_Vulkan_LoadLibrary(NULL), "Loading Vulkan Library");
	vk_chk(volkInitialize(), "Initializing Volk");
	vk_create_instance(ren);
	vk_init_debug_messenger(ren);
	vk_select_physical_device(ren);
	vk_create_device(ren);
	vk_init_vma(ren);
	sdl_create_window(ren, win);
	vk_create_swapchain(ren, win);
	vk_create_depth_buffer(ren, win);
	vk_create_uniform_buffers(ren);
	vk_create_sync_objects(ren);
	vk_allocate_command_buffers(ren);
	vk_load_texture(ren);
	vk_load_model(ren);
	vk_load_shaders(ren);
	vk_create_descriptor_pool(ren);
	vk_create_descriptor_set_layout(ren);
	vk_create_descriptor_sets(ren);
	vk_create_pipeline(ren);
}

void vk_cleanup(struct render_state *ren, struct window *win)
{
	vk_chk(vkDeviceWaitIdle(ren->device), "Waiting for idle device");

	for (u32 i = 0; i < FIF; i++) {
		vkDestroyFence(ren->device, ren->fences[i], NULL);
		vkDestroySemaphore(ren->device, ren->img_sems[i], NULL);
		vmaDestroyBuffer(ren->allocator, ren->ubuf[i].buf, ren->ubuf[i].alloc);
		vmaDestroyBuffer(ren->allocator, ren->test_uniform_buf[i].buf, ren->test_uniform_buf[i].alloc);
	}

	vmaDestroyImage(ren->allocator, ren->depth_image.image, ren->depth_image.alloc);
	vkDestroyImageView(ren->device, ren->depth_image.view, NULL);

	for (u32 i = 0; i < ren->swap_count; i++) {
		vkDestroySemaphore(ren->device, ren->ren_sems[i], NULL);
		vkDestroyImageView(ren->device, ren->swap_images[i].view, NULL);
	}

	vmaDestroyBuffer(ren->allocator, ren->vert_buff, ren->vert_alloc);

	vkDestroyImageView(ren->device, ren->tex_image.view, NULL);
	vmaDestroyImage(ren->allocator, ren->tex_image.image, ren->tex_image.alloc);

	vkDestroyDescriptorSetLayout(ren->device, ren->set_layout_buf, NULL);
	vkDestroyDescriptorPool(ren->device, ren->desc_pool, NULL);
	vkDestroyPipelineLayout(ren->device, ren->pipeline_layout, NULL);
	vkDestroyPipeline(ren->device, ren->pipeline, NULL);
	vkDestroySwapchainKHR(ren->device, ren->swapchain, NULL);
	vkDestroySurfaceKHR(ren->instance, win->surf, NULL);
	vkDestroyCommandPool(ren->device, ren->cmd_pool, NULL);
	vkDestroyShaderModule(ren->device, ren->shader, NULL);
	vmaDestroyAllocator(ren->allocator);
	SDL_DestroyWindow(win->p_win);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_Quit();
	vkDestroyDevice(ren->device, NULL);
	vkDestroyDebugUtilsMessengerEXT(ren->instance, ren->debug_messenger, nullptr);
	vkDestroyInstance(ren->instance, NULL);
}

float lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

vec2s vec2_lerp(vec2s a, vec2s b, float t)
{
	return (vec2s){ { lerp(a.x, b.x, t), lerp(a.y, b.y, t) } };
}

void vk_update_uniforms(struct render_state *ren)
{
	vec2s mouse_pos;
	SDL_GetMouseState(&mouse_pos.x, &mouse_pos.y);
	ren->player_pos = vec2_lerp(ren->player_pos, mouse_pos, 0.01f);
	ren->uniforms.pos = ren->player_pos;
	ren->uniforms.color.r = 0.0f;
	ren->uniforms.color.g = 1.0f;
	ren->uniforms.color.b = 0.0f;
	ren->uniforms.color.a = 1.0f;

	ren->test_uniforms.test_color.r = 0.0f;
	ren->test_uniforms.test_color.g = 0.0f;
	ren->test_uniforms.test_color.b = 0.2f;
	ren->test_uniforms.test_color.a = 1.0f;
}

void vk_draw_frame(struct render_state *ren, struct window *win)
{
	u32 frame = ren->frame_index;
	vk_chk(vkWaitForFences(ren->device, 1, &ren->fences[frame], true, UINT64_MAX), NULL);
	vk_chk(vkResetFences(ren->device, 1, &ren->fences[frame]), NULL);
	vkAcquireNextImageKHR(ren->device, ren->swapchain, UINT64_MAX, ren->img_sems[frame], VK_NULL_HANDLE,
			      &ren->image_index);

	//help me
	vk_update_uniforms(ren);
	memcpy(ren->ubuf[frame].info.pMappedData, &ren->uniforms, sizeof(struct uniform_data));
	memcpy(ren->test_uniform_buf[frame].info.pMappedData, &ren->test_uniforms, sizeof(struct uniform_test));

	VkCommandBuffer cb = ren->cmds[frame];
	vk_chk(vkResetCommandBuffer(cb, 0), NULL);
	VkCommandBufferBeginInfo cbi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vk_chk(vkBeginCommandBuffer(cb, &cbi), NULL);

	VkImageMemoryBarrier2 outputBarriers[2] = {
		(VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = ren->swap_images[ren->image_index].image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					      .levelCount = 1,
					      .layerCount = 1 },
		},
		(VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = ren->depth_image.image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
					      .levelCount = 1,
					      .layerCount = 1 },
		},
	};

	VkDependencyInfo barrierDependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 2,
		.pImageMemoryBarriers = outputBarriers,
	};

	vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);

	VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ren->swap_images[ren->image_index].view,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { { 0.0f, 0.0f, 0.2f, 1.0f } } },
	};

	VkRenderingAttachmentInfo depthAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ren->depth_image.view,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue = { .depthStencil = { 1.0f, 0 } },
	};

	VkRenderingInfo renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = { .extent = { .width = win->w, .height = win->h } },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = &depthAttachmentInfo,
	};

	vkCmdBeginRendering(cb, &renderingInfo);

	VkViewport vp = {
		.width = win->w,
		.height = win->h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport(cb, 0, 1, &vp);
	VkRect2D scissor = {
		.extent = { .width = win->w, .height = win->h },
	};

	vkCmdSetScissor(cb, 0, 1, &scissor);

	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->pipeline);
	VkDeviceSize vOffset = 0;

	//TODO: Bind texture descriptor sets
	// vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetTex, 0,
	// 			nullptr);

	vkCmdBindVertexBuffers(cb, 0, 1, &ren->vert_buff, &vOffset);
	vkCmdBindIndexBuffer(cb, ren->vert_buff, sizeof(vertices), VK_INDEX_TYPE_UINT32);
	vkCmdPushConstants(cb, ren->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress),
			   &ren->ubuf[frame].addr);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->pipeline_layout, 0, 1, &ren->set_test[frame],
				0, NULL);
	vkCmdDrawIndexed(cb, sizeof(indices) / sizeof(indices[0]), 1, 0, 0, 0);
	vkCmdEndRendering(cb);

	VkImageMemoryBarrier2 barrierPresent = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = 0,
		.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.image = ren->swap_images[ren->image_index].image,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 },
	};

	VkDependencyInfo barrierPresentDependencyInfo = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrierPresent,
	};

	vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);

	vkEndCommandBuffer(cb);

	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ren->img_sems[frame],
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cb,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ren->ren_sems[ren->image_index],
	};

	vk_chk(vkQueueSubmit(ren->gfx_q, 1, &submitInfo, ren->fences[frame]), NULL);

	// frameIndex = (frameIndex + 1) % maxFramesInFlight;
	ren->frame_index = (ren->frame_index + 1) % FIF;

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ren->ren_sems[ren->image_index],
		.swapchainCount = 1,
		.pSwapchains = &ren->swapchain,
		.pImageIndices = &ren->image_index,
	};

	vkQueuePresentKHR(ren->gfx_q, &presentInfo);
}

void poll_events(struct window *win)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_EVENT_QUIT:
			win->should_close = true;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			win->should_close = true;
		}
	}
}

int main()
{
	struct render_state ren;
	struct window win;

	sdl_chk(SDL_Init(SDL_INIT_VIDEO), "Initializing");
	vk_init(&ren, &win);

	win.should_close = false;

	while (!win.should_close) {
		poll_events(&win);
		vk_draw_frame(&ren, &win);
	}

	vk_cleanup(&ren, &win);
	return 0;
}
