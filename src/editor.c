#include "imgui/dcimgui.h"
#include "imgui/dcimgui_impl_sdl3.h"
#include "imgui/dcimgui_impl_vulkan.h"

#include "log.h"
#include "types.h"

void imgui_init(struct render_state *ren, struct window *win)
{
	ImGuiContext *ctx = ImGui_CreateContext(NULL);
	ImGui_SetCurrentContext(ctx);
	ImGuiIO *io = ImGui_GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	cImGui_ImplSDL3_InitForVulkan(win->p_win);
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

void draw_debug(struct render_state *ren)
{
	ImGui_Begin("Debug", NULL, 0);
	ImGui_DragFloat3Ex("pos", &ren->entities[0].transform.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("rot", &ren->entities[0].transform.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("scale", &ren->entities[0].transform.scale.x, 0.01f, 0, 0, NULL, 0);

	ImGui_DragFloat3Ex("pos2", &ren->entities[1].transform.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("rot2", &ren->entities[1].transform.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("scale2", &ren->entities[1].transform.scale.x, 0.01f, 0, 0, NULL, 0);
	ImGui_End();
}

void draw_imgui(struct render_state *ren)
{
	bool show_demo = true;
	cImGui_ImplSDL3_NewFrame();
	cImGui_ImplVulkan_NewFrame();
	ImGui_NewFrame();
	ImGui_DockSpaceOverViewportEx(0, ImGui_GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, NULL);

	draw_debug(ren);
	ImGui_ShowDemoWindow(&show_demo);
	ImGui_Render();
	ren->imgui_draw_data = ImGui_GetDrawData();
	// ImGui_EndFrame();
}
