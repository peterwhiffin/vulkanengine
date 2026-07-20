#include "imgui/dcimgui.h"
#include "imgui/dcimgui_impl_sdl3.h"
#include "imgui/dcimgui_impl_vulkan.h"
#include "cglm/struct.h"

#include "log.h"
#include "types.h"
#include "transform.h"
#include "scene.h"

void imgui_init(struct render_state *ren, struct window *win)
{
	ImGuiContext *ctx = ImGui_CreateContext(NULL);
	ImGui_SetCurrentContext(ctx);
	ImGuiIO *io = ImGui_GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	cImGui_ImplSDL3_InitForVulkan(win->sdl_win);

	VkFormat fmt = VK_FORMAT_R8G8B8A8_SRGB;
	VkPipelineRenderingCreateInfoKHR rinfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &fmt,
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

void draw_editor_settings(struct editor *editor)
{
	ImGui_Begin("Editor Settings", NULL, 0);
	ImGui_CollapsingHeader("Editor Camera", ImGuiTreeNodeFlags_CollapsingHeader);
	struct camera *cam = &editor->cam->camera;
	float fov_deg = glm_deg(cam->fov);
	ImGui_DragFloatEx("FOV", &fov_deg, 0.01f, 0.01f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);
	cam->fov = glm_rad(fov_deg);
	ImGui_DragFloatEx("Near Plane", &cam->near, 0.01f, 0.01f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);
	ImGui_DragFloatEx("Far Plane", &cam->far, 0.01f, 0.01f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);

	ImGui_DragFloatEx("Look Sens", &editor->look_sens, 0.01f, 0.01f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);
	ImGui_DragFloatEx("Move Speed", &editor->move_speed, 0.01f, 0.01f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);

	struct transform *t = &editor->ren->entities[0].transform;
	ImGui_DragFloat3Ex("Light Dir", &editor->ren->light_dir.x, 0.01f, 0.0f, 0.0f, NULL,
			   ImGuiSliderFlags_ColorMarkers);

	ImGui_Text("%s%.0f", "FPS: ", editor->fps);
	ImGui_Text("%s%.2f%s", "Frame time: ", editor->frame_time, " ms");
	ImGui_End();
}

void draw_scene(struct editor *editor)
{
	ImGui_Begin("Scene", NULL, 0);
	struct entity *entities = editor->ren->entities;
	for (int i = 0; i < editor->ren->entity_count; i++) {
		struct entity *e = &entities[i];
		if (e == editor->cam)
			continue;

		ImGui_PushIDPtr(e);
		if (ImGui_SelectableEx(e->name, e == editor->selected_entity, 0, (ImVec2){ 0, 0 })) {
			editor->selected_entity = e;
		}
		ImGui_PopID();
	}

	if (ImGui_IsWindowHovered(0) && editor->input->actions[M1].state == STARTED) {
		ImGui_OpenPopup("hierarchyctx", 0);
	}

	if (ImGui_BeginPopup("hierarchyctx", 0)) {
		if (ImGui_MenuItem("New Entity")) {
			get_new_entity(editor->ren);
		}

		ImGui_EndPopup();
	}

	ImGui_End();
}

void draw_inspector(struct editor *editor)
{
	ImGui_Begin("Inspector", NULL, 0);
	if (editor->selected_entity != NULL) {
		struct entity *e = editor->selected_entity;
		if (ImGui_IsWindowHovered(0) && editor->input->actions[M1].state == STARTED) {
			ImGui_OpenPopup("entityctx", 0);
		}

		if (ImGui_BeginPopup("entityctx", 0)) {
			if (ImGui_MenuItemEx("Add Mesh Renderer", NULL, false, !(e->flags & MESH_RENDERER))) {
				entity_add_mesh_renderer(editor->ren, e, &editor->ren->meshes[0]);
			}

			ImGui_EndPopup();
		}

		if (ImGui_CollapsingHeader("Transform", 0)) {
			struct transform *t = &e->transform;
			ImGui_DragFloat3Ex("Pos", &t->pos.x, 0.01f, 0.0f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);
			ImGui_DragFloat3Ex("Rot", &t->rot.x, 0.01f, 0.0f, 0.0f, NULL, ImGuiSliderFlags_ColorMarkers);
			ImGui_DragFloat3Ex("Scale", &t->scale.x, 0.01f, 0.0f, 0.0f, NULL,
					   ImGuiSliderFlags_ColorMarkers);
			set_position(t, t->pos);
			set_rotation(t, t->rot);
			set_scale(t, t->scale);
		}

		if (e->flags & MESH_RENDERER) {
			struct mesh_renderer *mr = &e->mesh_renderer;
			if (ImGui_CollapsingHeader("Mesh Renderer", 0)) {
				if (ImGui_BeginCombo("Mesh", mr->mesh->name, 0)) {
					for (int i = 0; i < editor->ren->mesh_count; i++) {
						if (ImGui_Selectable(editor->ren->meshes[i].name)) {
							mr->mesh = &editor->ren->meshes[i];
						}
					}
					ImGui_EndCombo();
				}
			}
		}
	}

	ImGui_End();
}

void draw_imgui(struct editor *editor)
{
	bool show_demo = true;
	cImGui_ImplSDL3_NewFrame();
	cImGui_ImplVulkan_NewFrame();
	ImGui_NewFrame();
	ImGui_DockSpaceOverViewportEx(0, ImGui_GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, NULL);

	draw_editor_settings(editor);
	draw_scene(editor);
	draw_inspector(editor);
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
	c->proj.raw[1][1] *= -1;

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

		editor->cam_pitch += input->actions[MOUSE_DELTA].composite.y * editor->look_sens;
		editor->cam_yaw -= input->actions[MOUSE_DELTA].composite.x * editor->look_sens;

		vec3s target_rot = { editor->cam_pitch, editor->cam_yaw, 0.0f };

		// editor->game->set_rotation(editor->editor_cam->transform, target_rot);
		set_rotation(t, target_rot);

		vec3s forward = get_forward(t);
		vec3s right = get_right(t);

		vec3s move_dir = glms_vec3_add(glms_vec3_scale(forward, input->actions[WASD].composite.y),
					       glms_vec3_scale(right, input->actions[WASD].composite.x));

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

	if (editor->time_accum >= 1.0f) {
		editor->fps = editor->frame_count / editor->time_accum;
		editor->frame_time = (editor->time_accum / editor->frame_count) * 1000.0f;
		editor->time_accum = 0.0f;
		editor->frame_count = 0;
	} else {
		editor->time_accum += editor->ren->delta_time;
		editor->frame_count++;
	}
}
