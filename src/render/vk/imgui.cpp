#include <lunar/render/imgui_layer.hpp>
#include <lunar/debug.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "vk_frame.hpp"
#include "vk_render_device.hpp"
#include "vk_swapchain.hpp"

namespace lunar::Render
{
	namespace
	{
		constexpr uint32_t USER_TEXTURE_CAPACITY = 16;
	}

	ImGuiLayer::ImGuiLayer(RenderDevice& device, Swapchain& swapchain, Window_T& window) noexcept
		: device(device)
	{
		imp::VkRenderDevice& vk_device        = static_cast<imp::VkRenderDevice&>(device);
		imp::VkSwapchain&    vk_swapchain     = static_cast<imp::VkSwapchain&>(swapchain);
		const VkFormat       swapchain_format = imp::ToVkFormat(swapchain.getFormat());
		const uint32_t       image_count      = vk_swapchain.getImageCount();

		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForVulkan(window.glfwGetHandle(), true);

		ImGui_ImplVulkan_InitInfo init_info =
		{
			.Instance                    = vk_device.getInstance(),
			.PhysicalDevice              = vk_device.getDevice().physical_device,
			.Device                      = vk_device.getDevice(),
			.QueueFamily                 = vk_device.getGraphicsQueueFamily(),
			.Queue                       = vk_device.getGraphicsQueue(),
			.MinImageCount               = image_count,
			.ImageCount                  = image_count,
			.DescriptorPoolSize          = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + USER_TEXTURE_CAPACITY,
			.UseDynamicRendering         = true,
			.PipelineRenderingCreateInfo =
			{
				.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount    = 1,
				.pColorAttachmentFormats = &swapchain_format
			}
		};

		initialized = ImGui_ImplVulkan_Init(&init_info);
		if (initialized)
			return;

		DEBUG_ERROR("Failed to initialize the ImGui Vulkan backend");
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	ImGuiLayer::~ImGuiLayer() noexcept
	{
		if (!initialized)
			return;

		device.waitIdle();
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::beginFrame()
	{
		if (!initialized)
			return;

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::endFrame()
	{
		if (initialized)
			ImGui::Render();
	}

	void ImGuiLayer::record(CommandList& commands)
	{
		ImDrawData* draw_data = initialized ? ImGui::GetDrawData() : nullptr;
		if (draw_data == nullptr)
			return;

		ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<imp::VkCommandList&>(commands).getHandle());
	}
}
