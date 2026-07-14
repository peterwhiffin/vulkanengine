#pragma once
#include "stdint.h"

#include "SDL3/SDL_video.h"
#include "volk.h"
#include "vk_mem_alloc.h"
#include "cglm/types-struct.h"

typedef uint8_t u8;
typedef uint32_t u32;

struct vertex {
	vec3s pos;
	vec3s norm;
	vec3s uv;
};

struct uniform_buffer {
	VmaAllocation alloc;
	VmaAllocationInfo info;
	VkBuffer buf;
	VkDeviceAddress addr;
};

struct uniform_data {
	vec4s color;
	vec2s pos;
	float time;

	// mat4s proj;
	// mat4s view;
	// mat4s model[3];
	// vec4s light_pos;
};

struct uniform_test {
	vec4s test_color;
};

struct image {
	VkImage image;
	VkImageView view;
	VmaAllocation alloc;
};

struct render_state {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue gfx_q;
	VkFormat swap_fmt;
	VkFormat depth_fmt;
	VkSwapchainKHR swapchain;
	VkCommandPool cmd_pool;
	VkCommandBuffer *cmds;
	VkFence *fences;
	VkSemaphore *img_sems;
	VkSemaphore *ren_sems;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkDescriptorSetLayout set_layout_tex;
	VkDescriptorSetLayout set_layout_buf;
	VkDescriptorPool desc_pool;
	VkDescriptorSet set_tex;
	VkDescriptorSet *set_test;
	VkBuffer vert_buff;
	VkShaderModule shader;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VmaAllocation vert_alloc;
	struct uniform_buffer *ubuf;
	struct uniform_buffer *test_uniform_buf;
	struct uniform_data uniforms;
	struct uniform_test test_uniforms;
	u32 swap_count;
	struct image *swap_images;
	struct image depth_image;
	struct image tex_image;
	VmaAllocator allocator;
	u32 gfx_q_fam;
	u32 frame_index;
	u32 image_index;
	bool update_swap;
	vec2s player_pos;
};

struct window {
	SDL_Window *p_win;
	VkSurfaceKHR surf;
	u32 w;
	u32 h;
	bool should_close;
};
