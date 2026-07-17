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
		exit(0);
	} else {
		printf("ImGui init vulkan SUCCESS!!\n");
	}
}

void draw_debug(struct render_state *ren)
{
	ImGui_Begin("Debug", NULL, 0);
	ImGui_DragFloat3Ex("pos", &ren->sponza_transform.pos.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("rot", &ren->sponza_transform.rot.x, 0.01f, 0, 0, NULL, 0);
	ImGui_DragFloat3Ex("scale", &ren->sponza_transform.scale.x, 0.01f, 0, 0, NULL, 0);
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

// void initImGui()
// {
// 	IMGUI_CHECKVERSION();
// 	ImGui::CreateContext();
// 	ImGuiIO &io = ImGui::GetIO();
// 	(void)io;
// 	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
// 	ImGui_ImplSDL3_InitForVulkan(window);
// 	ImGui_ImplVulkan_InitInfo initInfo = {};
// 	vk::Format depthFormat = findDepthFormat();
//
// 	initInfo.Instance = *instance;
// 	initInfo.PhysicalDevice = *physicalDevice;
// 	initInfo.Device = *device;
// 	initInfo.QueueFamily = queueIndex;
// 	initInfo.Queue = *graphicsQueue;
// 	initInfo.DescriptorPoolSize = 10;
// 	initInfo.UseDynamicRendering = true;
// 	initInfo.PipelineInfoMain.MSAASamples = static_cast<VkSampleCountFlagBits>(msaaSamples);
// 	initInfo.PipelineInfoMain.PipelineRenderingCreateInfo =
// 		vk::PipelineRenderingCreateInfo{ .colorAttachmentCount = 1,
// 						 .pColorAttachmentFormats = &swapchainSurfaceFormat.format,
// 						 .depthAttachmentFormat = depthFormat };
//
// 	initInfo.MinImageCount = 3;
// 	initInfo.ImageCount = 3;
// 	ImGui_ImplVulkan_Init(&initInfo);
// 	debugSettings.color = { 1.0f, 1.0f, 1.0f, 1.0f };
// }

// void draw_imgui()
// {
// 	bool showDemo = true;
//
// 	ImGui_ImplVulkan_NewFrame();
// 	ImGui_ImplSDL3_NewFrame();
// 	ImGui::NewFrame();
// 	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, NULL);
//
// 	ImGui::ShowDemoWindow(&showDemo);
// 	updateEditor();
//
// 	ImGui::Render();
// 	draw_data = ImGui::GetDrawData();
// }
//
// void cleanupImgui()
// {
// 	ImGui_ImplVulkan_Shutdown();
// 	ImGui_ImplSDL3_Shutdown();
// 	ImGui::DestroyContext();
// }
//
// ImGui_ImplVulkan_RenderDrawData(draw_data, *commandBuffers[frameIndex]);
