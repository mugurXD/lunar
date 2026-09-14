#pragma once
#include <lunar/render/render_device.hpp>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace lunar::Render::imp
{
	class VkSwapchain final : public Swapchain
	{
	public:
		VkSwapchain(const vkb::Device& device, const Window_T& window) noexcept;
		~VkSwapchain() noexcept override;

	private:
		vkb::Swapchain swapchain = {};
	};
}
