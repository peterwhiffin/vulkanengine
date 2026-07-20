#include "assert.h"

#define CGLM_FORCE_LEFT_HANDED
#define VOLK_IMPLEMENTATION
#define VK_NO_PROTOTYPES
#include "volk.h"

#include "SDL3/SDL_vulkan.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "log.h"
#include "types.h"
#include "cglm/struct.h"

#include "editor.c"
#include "transform.c"
#include "scene.c"

bool const engine_validation_layers_enabled = true;
u32 const FIF = 2;

// clang-format off
float quad_verts[] = {
	-1.0f,-1.0f, 0.0f, 0.0f, 
	-1.0f, 1.0f, 0.0f, 1.0f,
	 1.0f,-1.0f, 1.0f, 0.0f,
	 1.0f, 1.0f, 1.0f, 1.0f
};
// clang-format on
u32 quad_indices[] = { 0, 1, 2, 3 };

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

void chk_swapchain(struct render_state *ren, VkResult result)
{
	if (result < VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			ren->update_swap = true;
			return;
		}

		LOG_ERR("%s", "Swapchain Error");
		exit(result);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
						    VkDebugUtilsMessageTypeFlagsEXT type,
						    VkDebugUtilsMessengerCallbackDataEXT const *pCallbackData,
						    void *pUserData)
{
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

	VkPhysicalDeviceVulkan11Features f11 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.shaderDrawParameters = VK_TRUE,
	};

	VkPhysicalDeviceVulkan12Features f12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &f11,
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
	SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	// SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
	sdl_chk(win->sdl_win = SDL_CreateWindow("Vulkan Engine", win->w, win->h, flags), "Creating Window");
	sdl_chk(SDL_Vulkan_CreateSurface(win->sdl_win, ren->instance, NULL, &win->surf), "Creating Vulkan Surface");
	sdl_chk(SDL_GetWindowSize(win->sdl_win, (int *)&win->w, (int *)&win->h), "Getting Window Size");
}

void vk_create_swapchain(struct render_state *ren, struct window *win, VkSwapchainKHR old)
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
		.oldSwapchain = old,
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
	ren->camera_uniform_buffer = malloc(sizeof(*ren->camera_uniform_buffer) * FIF);

	for (u32 i = 0; i < FIF; i++) {
		VkBufferCreateInfo bci = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.size = sizeof(struct per_frame_uniforms),
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

		vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &ren->camera_uniform_buffer[i].buf,
				       &ren->camera_uniform_buffer[i].alloc, &ren->camera_uniform_buffer[i].info),
		       "Allocating Uniform Buffer");
		vmaSetAllocationName(ren->allocator, ren->camera_uniform_buffer[i].alloc, "Uniform Buffer");

		VkBufferDeviceAddressInfo dai = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.pNext = NULL,
			.buffer = ren->camera_uniform_buffer[i].buf,
		};

		ren->camera_uniform_buffer[i].addr = vkGetBufferDeviceAddress(ren->device, &dai);
	}
}

void vk_create_entity_uniform_buffer(struct render_state *ren, struct entity *e)
{
	struct mesh_renderer *mr = &e->mesh_renderer;
	mr->buffs = malloc(sizeof(*mr->buffs) * FIF);

	for (int i = 0; i < FIF; i++) {
		VkBufferCreateInfo bci = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.size = sizeof(struct entity_uniforms),
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

		vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &mr->buffs[i].buf, &mr->buffs[i].alloc,
				       &mr->buffs[i].info),
		       "Allocating Test Uniform Buffer");
		vmaSetAllocationName(ren->allocator, mr->buffs[i].alloc, "Entity Uniform Buffer");

		mr->buffs->addr = 0;
	}
}

void vk_create_sync_objects(struct render_state *ren)
{
	ren->fences = malloc(sizeof(*ren->fences) * FIF);
	ren->sem_img = malloc(sizeof(*ren->sem_img) * FIF);
	ren->sem_ren = malloc(sizeof(*ren->sem_ren) * ren->swap_count);

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
		vk_chk(vkCreateSemaphore(ren->device, &sci, NULL, &ren->sem_img[i]),
		       "Creating Image Acquired Semaphore");
	}

	for (u32 i = 0; i < ren->swap_count; i++) {
		vk_chk(vkCreateSemaphore(ren->device, &sci, NULL, &ren->sem_ren[i]),
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

struct image *vk_create_image(struct render_state *ren, VkImageCreateInfo *ici)
{
	struct image *img = &ren->images[ren->image_count];
	ren->image_count += 1;

	VmaAllocationCreateInfo iaci = {
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	vk_chk(vmaCreateImage(ren->allocator, ici, &iaci, &img->image, &img->alloc, NULL), "Allocating Image");
	vmaSetAllocationName(ren->allocator, img->alloc, "Render Target Image");

	return img;
}

struct image *vk_create_render_target(struct render_state *ren, u32 w, u32 h)
{
	struct image *img;

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
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	img = vk_create_image(ren, &ici);

	VkImageSubresourceRange sub = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = 1,
		.layerCount = 1,
	};

	VkImageViewCreateInfo vci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = img->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = ici.format,
		.subresourceRange = sub,
	};

	vk_chk(vkCreateImageView(ren->device, &vci, NULL, &img->view), "Creating Image View");

	return img;
}

void vk_load_texture(struct render_state *ren, char *file)
{
	int w, h, ch;
	stbi_uc *d = stbi_load(file, &w, &h, &ch, STBI_rgb_alpha);
	VkDeviceSize size = w * h * STBI_rgb_alpha;

	if (!d) {
		LOG_ERR("STB::Failed to load image: %s", file);
		exit(1);
	}

	struct image *img = &ren->textures[ren->texture_count];
	//take largest dim, log2 returns the number of times it can be divided by 2,
	//and take the floor for dims that aren't powers of 2, apparently.
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

	vk_chk(vmaCreateImage(ren->allocator, &ici, &iaci, &img->image, &img->alloc, NULL), "Allocating Texture Image");
	vmaSetAllocationName(ren->allocator, img->alloc, "Texture Image");

	VkImageSubresourceRange sub = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = mip_levels,
		.layerCount = 1,
	};

	VkImageViewCreateInfo vci = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = img->image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = ici.format,
		.subresourceRange = sub,
	};

	vk_chk(vkCreateImageView(ren->device, &vci, NULL, &img->view), "Creating Image View");
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
		.image = img->image,
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
	vkCmdCopyBufferToImage(cmd, staging_buff, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cpy);

	VkImageSubresourceRange rsub = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = mip_levels,
		.layerCount = 1,
	};

	VkImageMemoryBarrier2 read_bar = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		.image = img->image,
		.subresourceRange = rsub,

	};

	VkDependencyInfo read_dep = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &read_bar,
	};

	vkCmdPipelineBarrier2(cmd, &read_dep);
	vk_end_one_time_cmds(ren, cmd);
	vmaDestroyBuffer(ren->allocator, staging_buff, staging_alloc);
	ren->texture_count += 1;
}

void update_post_descriptors(struct render_state *ren)
{
	for (u32 i = 0; i < FIF; i++) {
		VkDescriptorImageInfo info = {
			.imageView = ren->post_images[i]->view,
			.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		};

		VkDescriptorImageInfo sample_info = {
			.sampler = ren->sampler,
		};

		VkWriteDescriptorSet write_images[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = ren->set_tex_post[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &info,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = ren->set_tex_post[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.pImageInfo = &sample_info,
			},
		};

		vkUpdateDescriptorSets(ren->device, 2, write_images, 0, NULL);
	}
}

void update_swapchain(struct render_state *ren, struct window *win)
{
	ren->update_swap = false;
	SDL_GetWindowSize(win->sdl_win, (int *)&win->w, (int *)&win->h);
	vkDeviceWaitIdle(ren->device);

	VkSwapchainKHR old_swap = ren->swapchain;
	vk_create_swapchain(ren, win, ren->swapchain);

	for (int i = 0; i < ren->swap_count; i++) {
		vkDestroySemaphore(ren->device, ren->sem_ren[i], NULL);
	}

	VkSemaphoreCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
	};

	for (int i = 0; i < ren->swap_count; i++) {
		vkCreateSemaphore(ren->device, &sci, NULL, &ren->sem_ren[i]);
	}

	vkDestroySwapchainKHR(ren->device, old_swap, NULL);
	vmaDestroyImage(ren->allocator, ren->depth_image.image, ren->depth_image.alloc);
	vkDestroyImageView(ren->device, ren->depth_image.view, NULL);

	vk_create_depth_buffer(ren, win);

	for (int i = 0; i < FIF; i++) {
		vmaDestroyImage(ren->allocator, ren->post_images[i]->image, ren->post_images[i]->alloc);
		vkDestroyImageView(ren->device, ren->post_images[i]->view, NULL);
		ren->image_count -= 1;
	}

	for (int i = 0; i < FIF; i++) {
		ren->post_images[i] = vk_create_render_target(ren, win->w, win->h);
	}

	update_post_descriptors(ren);
}
void get_dir_path(char *out, const char *path)
{
	char const *slash = strrchr(path, '/');
	char const *back_slash = strrchr(path, '\\');

	size_t len = 0;

	if (back_slash) {
		len = back_slash - path;
	} else if (slash) {
		len = slash - path;
	}

	sprintf(out, "%s", path);
	out[len + 1] = '\0';
}

void get_filename(char *out, const char *path)
{
	char const *slash = strrchr(path, '/');
	char const *back_slash = strrchr(path, '\\');

	size_t pos = 0;

	if (back_slash) {
		pos = back_slash - path + 1;
	} else if (slash) {
		pos = slash - path + 1;
	}

	sprintf(out, "%s", path + pos);
}

struct mesh *get_new_mesh(struct render_state *ren)
{
	struct mesh *m = &ren->meshes[ren->mesh_count];
	ren->mesh_count += 1;
	*m = (struct mesh){ 0 };
	return m;
}

cgltf_accessor *gltf_find_accessor(cgltf_primitive *prim, cgltf_attribute_type type, cgltf_size index)
{
	for (size_t i = 0; i < prim->attributes_count; i++) {
		cgltf_attribute *att = &prim->attributes[i];

		if (att->type == type && att->index == index)
			return att->data;
	}

	return NULL;
}

struct mesh *vk_load_model(struct render_state *ren, char *path)
{
	cgltf_options opt = { 0 };
	cgltf_data *data = NULL;
	cgltf_result result = cgltf_parse_file(&opt, path, &data);
	struct mesh *my_mesh = get_new_mesh(ren);

	my_mesh->vertex_offset = 0;

	size_t vert_count = 0;
	size_t ind_count = 0;
	size_t prim_count = 0;
	size_t img_offset = ren->texture_count;

	char load_path[512];
	get_dir_path(load_path, path);
	get_filename(my_mesh->name, path);

	if (result == cgltf_result_success) {
		if (cgltf_validate(data) != cgltf_result_success)
			LOG_WARN("CGLTF::Failed to validate gltf file: %s", path);

		result = cgltf_load_buffers(&opt, data, path);

		for (size_t i = 0; i < data->images_count; i++) {
			char image_path[1024];
			cgltf_image *img = &data->images[i];
			snprintf(image_path, 1024, "%s%s", load_path, img->uri);
			vk_load_texture(ren, image_path);
		}

		for (size_t i = 0; i < data->meshes_count; i++) {
			cgltf_mesh *mesh = &data->meshes[i];
			prim_count = mesh->primitives_count;
			for (size_t j = 0; j < mesh->primitives_count; j++) {
				cgltf_primitive *prim = &mesh->primitives[j];
				vert_count += prim->attributes[0].data->count;
				ind_count += prim->indices->count;
			}
		}

		struct vertex *verts = malloc(sizeof(*verts) * vert_count);
		u32 *indices = malloc(sizeof(u32) * ind_count);
		my_mesh->submeshes = malloc(sizeof(*my_mesh->submeshes) * prim_count);
		my_mesh->submesh_count = prim_count;

		u32 vert_offset = 0;
		u32 ind_offset = 0;

		for (size_t i = 0; i < data->meshes_count; i++) {
			cgltf_mesh *mesh = &data->meshes[i];
			for (size_t j = 0; j < mesh->primitives_count; j++) {
				cgltf_primitive *prim = &mesh->primitives[j];
				cgltf_accessor *acc = gltf_find_accessor(prim, cgltf_attribute_type_position, 0);

				for (size_t k = 0; k < acc->count; k++) {
					struct vertex *v = &verts[vert_offset + k];
					cgltf_accessor_read_float(acc, k, v->pos.raw, 3);
				}

				acc = gltf_find_accessor(prim, cgltf_attribute_type_texcoord, 0);
				for (size_t k = 0; k < acc->count; k++) {
					struct vertex *v = &verts[vert_offset + k];
					cgltf_accessor_read_float(acc, k, v->uv.raw, 2);
				}

				acc = gltf_find_accessor(prim, cgltf_attribute_type_normal, 0);
				for (size_t k = 0; k < acc->count; k++) {
					struct vertex *v = &verts[vert_offset + k];
					cgltf_accessor_read_float(acc, k, v->norm.raw, 3);
				}

				for (size_t k = 0; k < prim->indices->count; k++) {
					u32 index = cgltf_accessor_read_index(prim->indices, k);
					indices[ind_offset + k] = vert_offset + index;
				}

				u32 image_index =
					prim->material->pbr_metallic_roughness.base_color_texture.texture->image -
					data->images;

				my_mesh->submeshes[j].tex = &ren->textures[img_offset + image_index];
				my_mesh->submeshes[j].tex_index = img_offset + image_index;
				my_mesh->submeshes[j].index_offset = ind_offset;
				my_mesh->submeshes[j].index_count = prim->indices->count;

				vert_offset += acc->count;
				ind_offset += prim->indices->count;
			}
		}

		cgltf_free(data);

		VkDeviceSize v_size = sizeof(struct vertex) * vert_count;
		VkDeviceSize i_size = sizeof(u32) * ind_count;

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

		vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &my_mesh->buff, &my_mesh->alloc, &info),
		       "Allocating Sponza Vertex Buffer");
		vmaSetAllocationName(ren->allocator, my_mesh->alloc, "Sponza Vertex Buffer");

		memcpy(info.pMappedData, verts, v_size);
		memcpy(((char *)info.pMappedData) + v_size, indices, i_size);
		my_mesh->ind_offset = v_size;
		printf("ind offset: %lu\n", v_size);
		// ren->sponza_i_offset = v_size;
		// ren->sponza_index_count = ind_count;
	} else {
		LOG_ERR("CGLTF::Failed to load model: %s", path);
		return NULL;
	}

	return my_mesh;
}

void vk_create_quad(struct render_state *ren)
{
	VkDeviceSize v_size = sizeof(quad_verts);
	VkDeviceSize i_size = sizeof(quad_indices);

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

	vk_chk(vmaCreateBuffer(ren->allocator, &bci, &aci, &ren->quad_mesh.buff, &ren->quad_mesh.alloc, &info),
	       "Allocating Vertex Buffer");
	vmaSetAllocationName(ren->allocator, ren->quad_mesh.alloc, "Quad Vertex Buffer");

	memcpy(info.pMappedData, quad_verts, v_size);
	memcpy(((char *)info.pMappedData) + v_size, quad_indices, i_size);
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

	vk_chk(vkCreateShaderModule(ren->device, &sci, NULL, &ren->default_shader), "Creating Shader Module");

	s = 0;
	c = read_file("post.spv", &s);
	VkShaderModuleCreateInfo sci_post = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = s,
		.pCode = (u32 *)c,
	};

	vk_chk(vkCreateShaderModule(ren->device, &sci_post, NULL, &ren->post_shader), "Creating Shader Module");
}

void vk_create_descriptor_pool(struct render_state *ren)
{
	VkDescriptorPoolSize pool_sizes[3];

	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 10000;

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	pool_sizes[1].descriptorCount = 10000;

	pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	pool_sizes[2].descriptorCount = 100;

	VkDescriptorPoolCreateInfo pci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 10000,
		.poolSizeCount = 3,
		.pPoolSizes = pool_sizes,
	};

	vk_chk(vkCreateDescriptorPool(ren->device, &pci, NULL, &ren->desc_pool), "Creating Descriptor Pool");
}

void vk_create_entity_descriptor_sets(struct render_state *ren, struct entity *e)
{
	struct mesh_renderer *mr = &e->mesh_renderer;
	u32 num_sets = FIF;
	VkDescriptorSetLayout *layouts = malloc(sizeof(*layouts) * num_sets);
	mr->sets = malloc(sizeof(*mr->sets) * num_sets);

	for (u32 i = 0; i < num_sets; i++) {
		layouts[i] = ren->set_layout_entity;
	}

	VkDescriptorSetAllocateInfo ai = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ren->desc_pool,
		.descriptorSetCount = num_sets,
		.pSetLayouts = layouts,
	};

	vk_chk(vkAllocateDescriptorSets(ren->device, &ai, mr->sets), "Allocating Descriptor Sets");

	for (u32 i = 0; i < FIF; i++) {
		VkDescriptorBufferInfo binfo = {
			.buffer = mr->buffs[i].buf,
			.offset = 0,
			.range = sizeof(struct entity_uniforms),
		};
		VkWriteDescriptorSet writes[1] = { 0 };

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = mr->sets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &binfo;

		vkUpdateDescriptorSets(ren->device, 1, writes, 0, NULL);
	}

	free(layouts);
}

void vk_create_descriptor_sets(struct render_state *ren)
{
	u32 num_sets = FIF;

	VkDescriptorSetAllocateInfo ai_tex = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ren->desc_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &ren->set_layout_tex,
	};

	vk_chk(vkAllocateDescriptorSets(ren->device, &ai_tex, &ren->set_tex), "Allocating Descriptor Sets");

	VkDescriptorImageInfo image_infos[512] = { 0 };

	for (u32 i = 0; i < ren->texture_count; i++) {
		image_infos[i].imageView = ren->textures[i].view;
		image_infos[i].imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}

	VkDescriptorImageInfo sampler_info = {
		.sampler = ren->sampler,
	};

	VkWriteDescriptorSet write_image[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ren->set_tex,
			.dstBinding = 0,
			.descriptorCount = ren->texture_count,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.pImageInfo = image_infos,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = ren->set_tex,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.pImageInfo = &sampler_info,
		},
	};

	vkUpdateDescriptorSets(ren->device, 2, write_image, 0, NULL);

	ren->set_tex_post = malloc(sizeof(*ren->set_tex_post) * FIF);

	VkDescriptorSetLayout l[2] = {
		ren->set_layout_tex_post,
		ren->set_layout_tex_post,
	};

	VkDescriptorSetAllocateInfo ai_post = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = ren->desc_pool,
		.descriptorSetCount = FIF,
		.pSetLayouts = l,
	};

	vk_chk(vkAllocateDescriptorSets(ren->device, &ai_post, ren->set_tex_post), "Allocating Descriptor Sets");

	for (u32 i = 0; i < FIF; i++) {
		VkDescriptorImageInfo info = {
			.imageView = ren->post_images[i]->view,
			.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		};

		VkDescriptorImageInfo sample_info = {
			.sampler = ren->sampler,
		};

		VkWriteDescriptorSet write_images[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = ren->set_tex_post[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &info,
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = ren->set_tex_post[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.pImageInfo = &sample_info,
			},
		};

		vkUpdateDescriptorSets(ren->device, 2, write_images, 0, NULL);
	}
}

void vk_create_sampler(struct render_state *ren)
{
	VkSamplerCreateInfo sci = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.minFilter = VK_FILTER_LINEAR,
		.magFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 8.0f,
		.maxLod = 0,
	};

	vk_chk(vkCreateSampler(ren->device, &sci, NULL, &ren->sampler), "Creating Sampler");
}

void vk_create_set_layouts(struct render_state *ren)
{
	VkDescriptorSetLayoutBinding bindings[5] = { 0 };

	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].descriptorCount = ren->texture_count;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[2].binding = 0;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	bindings[3].binding = 0;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[4].binding = 1;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 2,
		.pBindings = &bindings[0],
	};

	vk_chk(vkCreateDescriptorSetLayout(ren->device, &dci, NULL, &ren->set_layout_tex),
	       "Creating Descriptor Layout");

	dci.bindingCount = 1;
	dci.pBindings = &bindings[2];

	// dci = {
	// 	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	// 	.bindingCount = 1,
	// 	.pBindings = &bindings[3],
	// };

	vk_chk(vkCreateDescriptorSetLayout(ren->device, &dci, NULL, &ren->set_layout_entity),
	       "Creating Descriptor Layout");

	dci.bindingCount = 2;
	dci.pBindings = &bindings[3];
	vk_chk(vkCreateDescriptorSetLayout(ren->device, &dci, NULL, &ren->set_layout_tex_post),
	       "Creating Descriptor Layout");
}

void vk_create_post_pipeline(struct render_state *ren)
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.size = sizeof(vec4s),
	};

	VkDescriptorSetLayout layouts[1] = {
		ren->set_layout_tex_post,
		// ren->set_layout_entity,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange,
	};

	vk_chk(vkCreatePipelineLayout(ren->device, &pipelineLayoutCI, nullptr, &ren->post_pipeline_layout),
	       "Creating Pipeline Layout");

	// VkVertexInputBindingDescription vertexBinding = {
	// 	.binding = 0,
	// 	.stride = sizeof(float) * 8,
	// 	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	// };

	// VkVertexInputAttributeDescription vertexAttributes[3] = {
	// 	{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
	// 	{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(float) * 3 },
	// 	{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 6 },
	// };

	VkPipelineVertexInputStateCreateInfo vertexInputState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = NULL,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = NULL,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_VERTEX_BIT,
		  .module = ren->post_shader,
		  .pName = "vertMain" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		  .module = ren->post_shader,
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
		.depthTestEnable = VK_FALSE,
		.depthWriteEnable = VK_FALSE,
	};

	VkFormat fmt = VK_FORMAT_R8G8B8A8_SRGB;
	VkPipelineRenderingCreateInfo renderingCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &ren->swap_fmt,
		// .pColorAttachmentFormats = &fmt,
	};

	VkPipelineColorBlendAttachmentState blendAttachment = { .colorWriteMask = 0xF };

	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
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
		.layout = ren->post_pipeline_layout,
	};

	vk_chk(vkCreateGraphicsPipelines(ren->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &ren->post_pipeline),
	       "Creating Pipeline");
}

void vk_create_pipeline(struct render_state *ren)
{
	VkPushConstantRange pushConstantRange = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.size = sizeof(struct push_constants),
	};

	VkDescriptorSetLayout layouts[2] = {
		ren->set_layout_tex,
		ren->set_layout_entity,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = layouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange,
	};

	vk_chk(vkCreatePipelineLayout(ren->device, &pipelineLayoutCI, nullptr, &ren->default_pipeline_layout),
	       "Creating Pipeline Layout");

	VkVertexInputBindingDescription vertexBinding = {
		.binding = 0,
		.stride = sizeof(float) * 8,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	VkVertexInputAttributeDescription vertexAttributes[3] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(float) * 3 },
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 6 },
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBinding,
		.vertexAttributeDescriptionCount = 3,
		.pVertexAttributeDescriptions = vertexAttributes,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_VERTEX_BIT,
		  .module = ren->default_shader,
		  .pName = "vertMain" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		  .module = ren->default_shader,
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

	VkFormat fmt = VK_FORMAT_R8G8B8A8_SRGB;

	VkPipelineRenderingCreateInfo renderingCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &fmt,
		.depthAttachmentFormat = ren->depth_fmt,
	};

	VkPipelineColorBlendAttachmentState blendAttachment = { .colorWriteMask = 0xF };

	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment,
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.lineWidth = 1.0f,
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
		.layout = ren->default_pipeline_layout,
	};

	vk_chk(vkCreateGraphicsPipelines(ren->device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &ren->default_pipeline),
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
	vk_create_swapchain(ren, win, NULL);
	//TODO: This is dumb
	ren->post_images = malloc(sizeof(*ren->post_images) * FIF);
	for (int i = 0; i < FIF; i++) {
		ren->post_images[i] = vk_create_render_target(ren, win->w, win->h);
		// ren->post_images[1] = vk_create_render_target(ren, win->w, win->h);
	}
	vk_create_depth_buffer(ren, win);
	vk_create_uniform_buffers(ren);
	vk_create_sync_objects(ren);
	vk_allocate_command_buffers(ren);
	vk_create_sampler(ren);
	vk_load_texture(ren, "../../res/texture.jpg");
	// vk_create_quad(ren);
	vk_load_shaders(ren);
	vk_load_model(ren, "../../res/models/sponza/Sponza.gltf");
	vk_load_model(ren, "../../../glTF-Sample-Assets/Models/Suzanne/glTF/Suzanne.gltf");
	vk_load_model(ren, "../../../glTF-Sample-Assets/Models/WaterBottle/glTF/WaterBottle.gltf");
	vk_load_model(ren, "../../../glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf");
	vk_create_descriptor_pool(ren);
	vk_create_set_layouts(ren);
	vk_create_descriptor_sets(ren);
	vk_create_pipeline(ren);
	vk_create_post_pipeline(ren);
}

void vk_cleanup(struct render_state *ren, struct window *win)
{
	vk_chk(vkDeviceWaitIdle(ren->device), "Waiting for idle device");

	for (u32 i = 0; i < FIF; i++) {
		vkDestroyFence(ren->device, ren->fences[i], NULL);
		vkDestroySemaphore(ren->device, ren->sem_img[i], NULL);
		vmaDestroyBuffer(ren->allocator, ren->camera_uniform_buffer[i].buf,
				 ren->camera_uniform_buffer[i].alloc);
	}

	vmaDestroyImage(ren->allocator, ren->depth_image.image, ren->depth_image.alloc);
	vkDestroyImageView(ren->device, ren->depth_image.view, NULL);

	for (u32 i = 0; i < ren->swap_count; i++) {
		vkDestroySemaphore(ren->device, ren->sem_ren[i], NULL);
		vkDestroyImageView(ren->device, ren->swap_images[i].view, NULL);
	}

	vmaDestroyBuffer(ren->allocator, ren->quad_mesh.buff, ren->quad_mesh.alloc);

	vkDestroySampler(ren->device, ren->sampler, NULL);

	vkDestroyDescriptorSetLayout(ren->device, ren->set_layout_tex, NULL);
	vkDestroyDescriptorPool(ren->device, ren->desc_pool, NULL);
	vkDestroyPipelineLayout(ren->device, ren->default_pipeline_layout, NULL);
	vkDestroyPipeline(ren->device, ren->default_pipeline, NULL);
	vkDestroySwapchainKHR(ren->device, ren->swapchain, NULL);
	vkDestroySurfaceKHR(ren->instance, win->surf, NULL);
	vkDestroyCommandPool(ren->device, ren->cmd_pool, NULL);
	vkDestroyShaderModule(ren->device, ren->default_shader, NULL);
	vmaDestroyAllocator(ren->allocator);
	SDL_DestroyWindow(win->sdl_win);
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

void vk_draw_frame(struct render_state *ren, struct window *win, struct entity *cam_entity)
{
	struct camera *cam = &cam_entity->camera;
	u32 frame = ren->frame_index;
	vk_chk(vkWaitForFences(ren->device, 1, &ren->fences[frame], true, UINT64_MAX), NULL);
	vk_chk(vkResetFences(ren->device, 1, &ren->fences[frame]), NULL);
	chk_swapchain(ren, vkAcquireNextImageKHR(ren->device, ren->swapchain, UINT64_MAX, ren->sem_img[frame],
						 VK_NULL_HANDLE, &ren->image_index));

	struct per_frame_uniforms cam_uniforms = {
		.proj = cam->proj,
		.view = cam->view,
		.light_dir =
			{
				ren->light_dir.x,
				ren->light_dir.y,
				ren->light_dir.z,
				0.0f,
			},
		.cam_pos =
			{
				cam_entity->transform.pos.x,
				cam_entity->transform.pos.y,
				cam_entity->transform.pos.z,
				0.0f,
			},
	};

	memcpy(ren->camera_uniform_buffer[frame].info.pMappedData, &cam_uniforms, sizeof(struct per_frame_uniforms));

	VkCommandBuffer cmd = ren->cmds[frame];
	vk_chk(vkResetCommandBuffer(cmd, 0), NULL);
	VkCommandBufferBeginInfo cbi = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vk_chk(vkBeginCommandBuffer(cmd, &cbi), NULL);

	VkImageMemoryBarrier2 outputBarriers[2] = {
		(VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = ren->post_images[frame]->image,
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

	vkCmdPipelineBarrier2(cmd, &barrierDependencyInfo);

	VkRenderingAttachmentInfo colorAttachmentInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ren->post_images[frame]->view,
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

	vkCmdBeginRendering(cmd, &renderingInfo);

	VkViewport vp = {
		.width = win->w,
		.height = win->h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport(cmd, 0, 1, &vp);
	VkRect2D scissor = {
		.extent = { .width = win->w, .height = win->h },
	};

	vkCmdSetScissor(cmd, 0, 1, &scissor);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->default_pipeline);

	for (u32 i = 0; i < ren->entity_count; i++) {
		struct entity *e = &ren->entities[i];
		if (!(e->flags & MESH_RENDERER))
			continue;

		struct mesh_renderer *mr = &e->mesh_renderer;
		struct mesh *m = e->mesh_renderer.mesh;

		struct entity_uniforms u = {
			.model = e->transform.world_transform,
			.normal_mat = mr->normal_matrix,
		};

		memcpy(mr->buffs[frame].info.pMappedData, &u, sizeof(struct entity_uniforms));

		vkCmdBindVertexBuffers(cmd, 0, 1, &m->buff, &m->vertex_offset);
		vkCmdBindIndexBuffer(cmd, m->buff, m->ind_offset, VK_INDEX_TYPE_UINT32);

		for (u32 k = 0; k < m->submesh_count; k++) {
			struct submesh *sm = &m->submeshes[k];

			VkDescriptorSet sets[2] = {
				ren->set_tex,
				mr->sets[frame],
			};

			struct push_constants pc = {
				.bda = ren->camera_uniform_buffer[frame].addr,
				.tex_ind = sm->tex_index,
			};

			vkCmdPushConstants(cmd, ren->default_pipeline_layout,
					   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc),
					   &pc);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->default_pipeline_layout, 0,
						2, sets, 0, NULL);
			vkCmdDrawIndexed(cmd, sm->index_count, 1, sm->index_offset, 0, 0);
		}
	}
	cImGui_ImplVulkan_RenderDrawData(ImGui_GetDrawData(), cmd);
	vkCmdEndRendering(cmd);

	VkImageMemoryBarrier2 outputBarriersPost[2] = {

		(VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
			.image = ren->post_images[frame]->image,
			.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					      .levelCount = 1,
					      .layerCount = 1 },
		},
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
	};

	VkDependencyInfo barrierDependencyInfoPost = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 2,
		.pImageMemoryBarriers = outputBarriersPost,
	};

	vkCmdPipelineBarrier2(cmd, &barrierDependencyInfoPost);

	VkRenderingAttachmentInfo colorAttachmentInfoPost = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = ren->swap_images[ren->image_index].view,
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = { { 0.0f, 0.0f, 0.2f, 1.0f } } },
	};

	VkRenderingInfo renderingInfoPost = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = { .extent = { .width = win->w, .height = win->h } },
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfoPost,
	};

	vkCmdBeginRendering(cmd, &renderingInfoPost);

	VkViewport vpPost = {
		.width = win->w,
		.height = win->h,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport(cmd, 0, 1, &vpPost);
	VkRect2D scissorPost = {
		.extent = { .width = win->w, .height = win->h },
	};

	vkCmdSetScissor(cmd, 0, 1, &scissorPost);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->post_pipeline);

	VkDescriptorSet sets[1] = {
		ren->set_tex_post[frame],
	};

	vec4s color_test = { 1.0f, 0.0f, 0.0f, 1.0f };

	vkCmdPushConstants(cmd, ren->post_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			   sizeof(vec4s), &color_test);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ren->post_pipeline_layout, 0, 1, sets, 0, NULL);
	vkCmdDraw(cmd, 3, 1, 0, 0);
	vkCmdEndRendering(cmd);

	VkImageMemoryBarrier2 barrierPresent = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask = VK_ACCESS_NONE,
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

	vkCmdPipelineBarrier2(cmd, &barrierPresentDependencyInfo);

	vkEndCommandBuffer(cmd);

	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ren->sem_img[frame],
		.pWaitDstStageMask = &waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &ren->sem_ren[ren->image_index],
	};

	vk_chk(vkQueueSubmit(ren->gfx_q, 1, &submitInfo, ren->fences[frame]), NULL);

	ren->frame_index = (ren->frame_index + 1) % FIF;

	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &ren->sem_ren[ren->image_index],
		.swapchainCount = 1,
		.pSwapchains = &ren->swapchain,
		.pImageIndices = &ren->image_index,
	};

	chk_swapchain(ren, vkQueuePresentKHR(ren->gfx_q, &presentInfo));
}

void poll_events(struct window *win, struct input *input, struct render_state *ren)
{
	input->actions[MOUSE_DELTA].composite = (vec2s){ 0.0f, 0.0f };

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		cImGui_ImplSDL3_ProcessEvent(&event);
		switch (event.type) {
		case SDL_EVENT_MOUSE_MOTION:
			input->actions[MOUSE_DELTA].composite = (vec2s){ -event.motion.xrel, event.motion.yrel };
		case SDL_EVENT_MOUSE_WHEEL:
			input->actions[MWHEEL].axis = event.wheel.y;
			break;
		case SDL_EVENT_QUIT:
			win->should_close = true;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			win->should_close = true;
		case SDL_EVENT_WINDOW_RESIZED:
			ren->update_swap = true;
		}
	}
}

void update_time(struct render_state *ren)
{
	float time = SDL_GetTicks() / 1000.0f;
	ren->delta_time = time - ren->last_time;
	ren->last_time = time;
}

void input_update(struct input *input)
{
	float oldX = input->relativeCursorPosition.x;
	float oldY = input->relativeCursorPosition.y;

	SDL_GetMouseState(&input->relativeCursorPosition.x, &input->relativeCursorPosition.y);
	SDL_MouseButtonFlags mouseButtonFlags =
		SDL_GetGlobalMouseState(&input->cursorPosition.x, &input->cursorPosition.y);

	input->actions[MWHEEL].axis = 0.0f;
	input->actions[M0].pushed = mouseButtonFlags & SDL_BUTTON_LMASK;
	input->actions[M1].pushed = mouseButtonFlags & SDL_BUTTON_RMASK;
	input->actions[DEL].pushed = input->sdl_keys[SDL_SCANCODE_DELETE];
	input->actions[D].pushed = input->sdl_keys[SDL_SCANCODE_D];
	input->actions[F].pushed = input->sdl_keys[SDL_SCANCODE_F];
	input->actions[P].pushed = input->sdl_keys[SDL_SCANCODE_P];
	input->actions[L].pushed = input->sdl_keys[SDL_SCANCODE_L];
	input->actions[SPACE].pushed = input->sdl_keys[SDL_SCANCODE_SPACE];
	input->actions[LSHIFT].pushed = input->sdl_keys[SDL_SCANCODE_LSHIFT];
	input->actions[LCTRL].pushed = input->sdl_keys[SDL_SCANCODE_LCTRL];
	input->actions[WASD].composite = (vec2s){ input->sdl_keys[SDL_SCANCODE_D] - input->sdl_keys[SDL_SCANCODE_A],
						  input->sdl_keys[SDL_SCANCODE_W] - input->sdl_keys[SDL_SCANCODE_S] };
	input->actions[ARROWS].composite = (vec2s){
		input->sdl_keys[SDL_SCANCODE_LEFT] - input->sdl_keys[SDL_SCANCODE_RIGHT],
		input->sdl_keys[SDL_SCANCODE_DOWN] - input->sdl_keys[SDL_SCANCODE_UP],
	};

	for (int i = 0; i < ACTION_COUNT; i++) {
		struct key_action *action = &input->actions[i];

		switch (action->state) {
		case STARTED:
			action->state = action->pushed ? ACTIVE : CANCELED;
			break;
		case ACTIVE:
			if (!action->pushed)
				action->state = CANCELED;
			break;
		case CANCELED:
			if (action->pushed)
				action->state = STARTED;
			break;
		}
	}
}

void print_matrix3(mat3s mat)
{
	printf("[%f][%f][%f]\n", mat.m00, mat.m01, mat.m02);
	printf("[%f][%f][%f]\n", mat.m10, mat.m11, mat.m12);
	printf("[%f][%f][%f]\n", mat.m20, mat.m21, mat.m22);
}

void update_transforms(struct render_state *ren)
{
	for (int i = 0; i < ren->entity_count; i++) {
		struct transform *t = &ren->entities[i].transform;
		if (t->is_dirty) {
			struct entity *e = &ren->entities[i];
			update_transform_matrices(t);

			if ((e->flags & MESH_RENDERER)) {
				e->mesh_renderer.normal_matrix =
					glms_mat4_transpose(glms_mat4_inv(e->transform.world_transform));
			}
		}
	}
}

int main()
{
	struct render_state ren = { 0 };
	struct window win = { 0 };
	struct editor editor = { 0 };
	struct input input = { 0 };

	editor.ren = &ren;
	editor.win = &win;
	editor.input = &input;

	ren.textures = malloc(sizeof(*ren.textures) * 1000);
	ren.meshes = malloc(sizeof(*ren.meshes) * 40000);
	ren.entities = malloc(sizeof(*ren.entities) * 40000);
	ren.images = malloc(sizeof(*ren.images) * 500);

	sdl_chk(SDL_Init(SDL_INIT_VIDEO), "Initializing");

	input.sdl_keys = SDL_GetKeyboardState(NULL);
	input.lock_mouse = SDL_SetWindowRelativeMouseMode;

	vk_init(&ren, &win);
	imgui_init(&ren, &win);
	scene_init(&ren);

	for (int i = 0; i < FIF; i++) {
		printf("Image 0x%llx\n", (unsigned long long)ren.post_images[i]->image);
	}

	editor.cam = get_new_entity(&ren);
	entity_add_camera(editor.cam);
	editor.cam->camera.aspect = 800.0f / 600.0f;
	editor.cam->camera.near = 0.1f;
	editor.cam->camera.far = 10000.0f;
	editor.look_sens = 1.0f;
	editor.move_speed = 10.0f;
	update_time(&ren);
	update_transforms(&ren);

	while (!win.should_close) {
		poll_events(&win, &input, &ren);
		update_time(&ren);
		input_update(&input);
		draw_imgui(&editor);
		editor_update(&editor);
		update_transforms(&ren);
		// update_camera_uniforms(&ren);
		vk_draw_frame(&ren, &win, editor.cam);
		if (ren.update_swap)
			update_swapchain(&ren, &win);
	}

	vk_cleanup(&ren, &win);
	return 0;
}
