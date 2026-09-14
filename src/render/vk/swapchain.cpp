#include "vk_swapchain.hpp"

#include <lunar/render/window.hpp>
#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	VkSwapchain::VkSwapchain(const vkb::Device& device, const Window_T& window) noexcept
	{
		auto swapchain_res = vkb::SwapchainBuilder { device }
			.set_desired_extent(window.getRenderWidth(), window.getRenderHeight())
			.build();

		if (!swapchain_res)
		{
			DEBUG_ERROR("Failed to create Vulkan swapchain: {}", swapchain_res.error().message());
			return;
		}

		this->swapchain = swapchain_res.value();
		DEBUG_LOG("Created swapchain ({}x{}, {} images).", swapchain.extent.width, swapchain.extent.height, swapchain.image_count);
	}

	VkSwapchain::~VkSwapchain() noexcept
	{
		if (swapchain.swapchain == VK_NULL_HANDLE)
			return;

		vkDeviceWaitIdle(swapchain.device);
		vkb::destroy_swapchain(swapchain);
	}
}
