#include <lunar/render/imp/vk/swapchain.hpp>

namespace lunar::Render::imp
{
	Swapchain::Swapchain(VkSurfaceKHR surface, vkb::Swapchain swapchain)
		: surface(surface), swapchain(swapchain)
	{
	}

	Swapchain::~Swapchain()
	{
		vkb::destroy_swapchain(swapchain);
	}
}