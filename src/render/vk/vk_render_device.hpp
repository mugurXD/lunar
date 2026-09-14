#pragma once
#include <lunar/render/render_device.hpp>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace lunar::Render::imp
{
	class VkRenderDevice final : public RenderDevice
	{
	public:
		VkRenderDevice(const RenderDeviceSettings& settings) noexcept;
		~VkRenderDevice() noexcept override;

		std::unique_ptr<Swapchain> createSwapchain(Window_T& window) override;

	private:
		Window_T*        presentWindow            = nullptr;
		vkb::Instance    instance                 = {};
		VkSurfaceKHR     surface                  = VK_NULL_HANDLE;
		vkb::Device      device                   = {};
		VkQueue          graphicsQueue            = VK_NULL_HANDLE;
		VkQueue          presentQueue             = VK_NULL_HANDLE;
		uint32_t         graphicsQueueFamilyIndex = 0;
		uint32_t         presentQueueFamilyIndex  = 0;
		VkCommandPool    commandPool              = VK_NULL_HANDLE;
		VkCommandBuffer  mainCommandBuffer        = VK_NULL_HANDLE;
	};
}
