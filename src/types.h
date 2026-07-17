#pragma once
#include "stdint.h"

#include "SDL3/SDL_video.h"
// #include "volk.h"
#include "vk_mem_alloc.h"

#include "cglm/types-struct.h"
// #include "cglm/struct/cam.h"
// #include "cglm/types.h"
// #include "cglm/util.h"

#include "imgui/dcimgui_impl_vulkan.h"
// #include "imgui/dcimgui.h"

typedef uint8_t u8;
typedef uint32_t u32;

struct transform {
	vec3s pos;
	vec3s rot;
	vec3s scale;
};

struct vertex {
	vec3s pos;
	vec3s norm;
	vec2s uv;
};

struct uniform_buffer {
	VmaAllocation alloc;
	VmaAllocationInfo info;
	VkBuffer buf;
	VkDeviceAddress addr;
};

struct image {
	VkImage image;
	VkImageView view;
	VmaAllocation alloc;
};

struct submesh {
	VkDeviceSize index_offset;
	VkDeviceSize index_count;
	// VkDescriptorSet *desc_sets;
	u32 tex_index;
	struct image *tex;
};

struct mesh {
	VkBuffer buff;
	VkDeviceSize ind_offset;
	VkDeviceSize vertex_offset;
	VmaAllocation alloc;
	struct submesh *submeshes;
	u32 submesh_count;
};

struct uniform_data {
	mat4s proj;
	mat4s view;
	vec4s color;
	vec2s pos;
	vec2s test;
	float time;
};

struct entity_uniforms {
	mat4s model;
};

struct uniform_test {
	u32 tex_index;
};

struct push_constants {
	VkDeviceAddress bda;
	u32 tex_ind;
};

struct material {
	u32 albedo_index;
	vec4s color;
};

struct mesh_renderer {
	struct mesh *mesh;
	struct entity_uniforms uniforms;
	struct uniform_buffer *buffs;
	VkDescriptorSet *sets;
	u32 material_index;
};

struct rigidbody {
	u32 id;
};

enum entity_flags {
	NONE = 0,
	MESH_RENDERER = 1 << 1,
	RIGIDBODY = 1 << 2,
	CAMERA = 1 << 3,
};

struct camera {
	mat4 proj;
	mat4 view;
	float fov;
	bool is_perspective;
};

struct entity {
	char name[128];
	u8 flags;
	struct transform transform;
	struct mesh_renderer mesh_renderer;
	struct rigidbody rigidbody;
	struct camera camera;
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
	VkDescriptorSetLayout set_layout_entity;
	VkDescriptorSetLayout set_layout_buf;
	VkDescriptorPool desc_pool;
	VkDescriptorSet set_tex;
	VkDescriptorSet *set_test;
	VkBuffer vert_buff;
	VkBuffer sponza_vert_buff;
	VkSampler sampler;
	VkShaderModule shader;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VmaAllocation vert_alloc;
	VmaAllocation sponza_vert_alloc;
	struct uniform_buffer *ubuf;
	struct uniform_buffer *test_uniform_buf;
	struct uniform_data uniforms;
	struct uniform_test test_uniforms;
	u32 swap_count;
	struct image *swap_images;
	struct image depth_image;
	// struct image tex_image;
	VmaAllocator allocator;
	u32 gfx_q_fam;
	u32 frame_index;
	u32 image_index;
	VkDeviceSize sponza_i_offset;
	VkDeviceSize sponza_index_count;
	struct mesh *meshes;
	u32 mesh_count;
	struct entity *entities;
	u32 entity_count;
	// struct mesh sponza_mesh;
	struct image *textures;

	VkDescriptorSet set_global;
	u32 texture_count;
	bool update_swap;
	vec2s player_pos;
	float last_time;
	float delta_time;

	// struct transform sponza_transform;
	ImDrawData *imgui_draw_data;
};

struct render_pass {
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorSet set_per_frame;
	VkRenderingAttachmentInfo attachments;
	VkRenderingInfo info;
};

struct window {
	SDL_Window *p_win;
	VkSurfaceKHR surf;
	u32 w;
	u32 h;
	bool should_close;
};
