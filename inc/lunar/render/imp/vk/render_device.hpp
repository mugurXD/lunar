#pragma once
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <optional>

#include <lunar/api.hpp>
#include <lunar/render/context.hpp>
#include <lunar/render/imp/vk/swapchain.hpp>

namespace lunar::Render::imp
{ 
	class LUNAR_API VkRenderDevice : public RenderDevice
	{
	public:
		VkRenderDevice() = delete;
		VkRenderDevice(const RenderDeviceSettings&) noexcept;
		~VkRenderDevice()                           noexcept;
	private:
		vkb::Instance            instance;
		vkb::Device              device;
		VkQueue                  graphicsQueue;
		VkQueue                  presentQueue;
		std::optional<Swapchain> swapchain;
	};
}
