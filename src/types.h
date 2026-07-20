#pragma once
#include "stdint.h"

#include "SDL3/SDL_video.h"
#include "vk_mem_alloc.h"
#include "cglm/types-struct.h"

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

enum action_type { BUTTON, AXIS, COMPOSITE };

enum action_state { CANCELED = 0, STARTED, ACTIVE };

struct key_action {
	enum action_type type;
	enum action_state state;
	union {
		vec2s composite;
		float axis;
		bool pushed;
	};
};

enum key_actions {
	MWHEEL = 0,
	MOUSE_DELTA,
	WASD,
	ARROWS,
	M0,
	M1,
	SPACE,
	LSHIFT,
	LCTRL,
	DEL,
	D,
	F,
	L,
	P,
	ESC,
	ACTION_COUNT
};

struct input {
	bool (*lock_mouse)(SDL_Window *, bool);
	const bool *sdl_keys;
	vec2s cursorPosition;
	vec2s relativeCursorPosition;
	struct key_action actions[ACTION_COUNT];
};

struct transform {
	mat4s world_transform;
	mat3s normal_matrix;
	vec3s pos;
	vec3s rot;
	vec3s scale;
	bool is_dirty;
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
	char name[128];
	VkBuffer buff;
	VkDeviceSize ind_offset;
	VkDeviceSize vertex_offset;
	VmaAllocation alloc;
	struct submesh *submeshes;
	u32 submesh_count;
};

struct per_frame_uniforms {
	mat4s proj;
	mat4s view;
	vec4s light_dir;
	vec4s cam_pos;
};

struct entity_uniforms {
	mat4s model;
	mat4s normal_mat;
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
	mat4s normal_matrix;
	// struct entity_uniforms uniforms;
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
	mat4s proj;
	mat4s view;

	float fov;
	float aspect;
	float near;
	float far;
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
	VkSemaphore *sem_img;
	VkSemaphore *sem_ren;

	VkDebugUtilsMessengerEXT debug_messenger;

	VkDescriptorSetLayout set_layout_tex;
	VkDescriptorSetLayout set_layout_tex_post;
	VkDescriptorSetLayout set_layout_entity;
	VkDescriptorPool desc_pool;
	VkDescriptorSet set_tex;
	VkDescriptorSet *set_tex_post;
	VkSampler sampler;

	VkShaderModule default_shader;
	VkShaderModule post_shader;
	VkPipelineLayout default_pipeline_layout;
	VkPipelineLayout post_pipeline_layout;
	VkPipeline default_pipeline;
	VkPipeline post_pipeline;

	VmaAllocator allocator;

	struct image *images;
	u32 image_count;

	struct mesh quad_mesh;
	struct uniform_buffer *camera_uniform_buffer;
	struct per_frame_uniforms camera_uniforms;
	struct image *swap_images;
	struct image **post_images;
	struct image depth_image;

	struct mesh *meshes;
	struct entity *entities;
	struct image *textures;
	u32 entity_count;
	u32 texture_count;
	u32 mesh_count;

	float last_time;
	float delta_time;

	u32 gfx_q_fam;
	u32 swap_count;
	u32 frame_index;
	u32 image_index;

	vec3s light_dir;

	bool update_swap;
};

struct render_pass {
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	VkDescriptorSet set_per_frame;
	VkRenderingAttachmentInfo attachments;
	VkRenderingInfo info;
};

struct window {
	SDL_Window *sdl_win;
	VkSurfaceKHR surf;
	u32 w;
	u32 h;
	bool should_close;
};

struct editor {
	struct render_state *ren;
	struct window *win;
	struct input *input;
	struct entity *cam;
	float cam_pitch;
	float cam_yaw;
	float look_sens;
	float move_speed;
	struct entity *selected_entity;
};
