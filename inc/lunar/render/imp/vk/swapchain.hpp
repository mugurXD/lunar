#pragma once
#include <lunar/api.hpp>
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

namespace lunar::Render::imp
{
	class LUNAR_API Swapchain
	{
	public:
		Swapchain(VkSurfaceKHR surface, vkb::Swapchain swapchain);
		~Swapchain();

	private:
		VkSurfaceKHR   surface   = VK_NULL_HANDLE;
		vkb::Swapchain swapchain;

	};
}
