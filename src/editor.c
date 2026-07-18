#include "imgui/dcimgui.h"
#include "imgui/dcimgui_impl_sdl3.h"
#include "imgui/dcimgui_impl_vulkan.h"
#include "cglm/struct.h"

#include "log.h"
#include "types.h"
#include "transform.h"

void imgui_init(struct render_state *ren, struct window *win)
{
	ImGuiContext *ctx = ImGui_CreateContext(NULL);
	ImGui_SetCurrentContext(ctx);
	ImGuiIO *io = ImGui_GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	cImGui_ImplSDL3_InitForVulkan(win->sdl_win);
	VkPipelineRenderingCreateInfoKHR rinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &ren->swap_fmt,
		.depthAttachmentFormat = ren->depth_fmt,
	};

	ImGui_ImplVulkan_PipelineInfo pinfo = {
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		.PipelineRenderingCreateInfo = rinfo,
	};

	ImGui_ImplVulkan_InitInfo initInfo = {
		.Instance = ren->instance,
		.PhysicalDevice = ren->physical_device,
		.Device = ren->device,
		.QueueFamily = ren->gfx_q_fam,
		.Queue = ren->gfx_q,
		.DescriptorPoolSize = 100,
		.UseDynamicRendering = true,
		.PipelineInfoMain = pinfo,
		.MinImageCount = 2,
		.ImageCount = 2,
	};

	if (!cImGui_ImplVulkan_Init(&initInfo)) {
		printf("ImGui failed to init vulkan!\n");
		// exit(0);
	} else {
		printf("ImGui init vulkan SUCCESS!!\n");
	}
}

void draw_debug(struct editor *editor)
{
	ImGui_Begin("Debug", NULL, 0);

	struct transform t = editor->cam->transform;
	ImGui_DragFloat3Ex("cam pos", &t.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("cam rot", &t.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("cam scale", &t.scale.x, 0.01f, 0, 0, NULL, 0);
	set_position(&editor->cam->transform, t.pos);
	set_rotation(&editor->cam->transform, t.rot);
	set_scale(&editor->cam->transform, t.scale);

	t = editor->ren->entities[0].transform;
	ImGui_DragFloat3Ex("pos1", &t.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("rot1", &t.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("scale1", &t.scale.x, 0.01f, 0, 0, NULL, 0);
	set_position(&editor->ren->entities[0].transform, t.pos);
	set_rotation(&editor->ren->entities[0].transform, t.rot);
	set_scale(&editor->ren->entities[0].transform, t.scale);

	t = editor->ren->entities[1].transform;
	ImGui_DragFloat3Ex("pos2", &t.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("rot2", &t.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("scale2", &t.scale.x, 0.01f, 0, 0, NULL, 0);
	set_position(&editor->ren->entities[1].transform, t.pos);
	set_rotation(&editor->ren->entities[1].transform, t.rot);
	set_scale(&editor->ren->entities[1].transform, t.scale);

	ImGui_End();
}

void draw_imgui(struct editor *editor)
{
	bool show_demo = true;
	cImGui_ImplSDL3_NewFrame();
	cImGui_ImplVulkan_NewFrame();
	ImGui_NewFrame();
	ImGui_DockSpaceOverViewportEx(0, ImGui_GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, NULL);

	draw_debug(editor);
	ImGui_ShowDemoWindow(&show_demo);
	ImGui_Render();
	// ren->imgui_draw_data = ImGui_GetDrawData();
	// ImGui_EndFrame();
}

static void editor_camera_update_proj(struct entity *e)
{
	struct camera *c = &e->camera;
	struct transform *t = &e->transform;

	c->proj = glms_perspective(c->fov, c->aspect, c->near, c->far);

	vec3s eye = t->pos;
	vec3s center = glms_vec3_add(eye, get_forward(t));
	vec3s up = get_up(t);

	c->view = glms_lookat(eye, center, up);
}

void editor_camera_update(struct editor *editor)
{
	struct input *input = editor->input;
	struct window *win = editor->win;
	struct transform *t = &editor->cam->transform;
	// struct scene *scene = editor->scene;

	if (input->actions[M1].state == STARTED) {
		input->lock_mouse(win->sdl_win, true);
	} else if (input->actions[M1].state == CANCELED) {
		input->lock_mouse(win->sdl_win, false);
	}

	if (input->actions[M1].state != CANCELED) {
		vec3s current_rot = t->rot;

		editor->cam_pitch -= input->actions[MOUSE_DELTA].composite.y * editor->look_sens;
		editor->cam_yaw -= input->actions[MOUSE_DELTA].composite.x * editor->look_sens;

		vec3s target_rot = { editor->cam_pitch, editor->cam_yaw, 0.0f };

		// editor->game->set_rotation(editor->editor_cam->transform, target_rot);
		set_rotation(t, target_rot);

		vec3s forward = get_forward(t);
		vec3s right = get_right(t);

		vec3s move_dir = glms_vec3_add(glms_vec3_scale(forward, input->actions[WASD].composite.y),
					       glms_vec3_scale(right, -input->actions[WASD].composite.x));

		vec3s new_pos =
			glms_vec3_add(t->pos, glms_vec3_scale(move_dir, editor->move_speed * editor->ren->delta_time));
		// editor->game->set_position(editor->editor_cam->transform, new_pos);
		set_position(t, new_pos);
	}
}

void editor_update(struct editor *editor)
{
	editor_camera_update(editor);
	editor_camera_update_proj(editor->cam);
}
