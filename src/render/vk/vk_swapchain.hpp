#pragma once
#include "vk_render_device.hpp"

#include <vector>

namespace lunar::Render::imp
{
	class VkSwapchain final : public Swapchain
	{
	public:
		VkSwapchain(VkRenderDevice& device, const Window_T& window) noexcept;
		~VkSwapchain() noexcept override;

		ImageHandle acquire(VkSemaphore acquire_semaphore);
		void        present(VkQueue present_queue);
		ImageHandle getAcquiredImage()    const;
		VkSemaphore getPresentSemaphore() const;

	private:
		bool build();
		void releaseImages();
		bool isOutdated() const;

		VkRenderDevice&          device;
		const Window_T&          window;
		vkb::Swapchain           swapchain         = {};
		VkExtent2D               requestedExtent   = {};
		std::vector<VkImageView> imageViews        = {};
		std::vector<ImageHandle> images            = {};
		std::vector<VkSemaphore> presentSemaphores = {};
		uint32_t                 imageIndex        = 0;
		bool                     outdated          = false;
	};
}
