#include "vk_swapchain.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/render/window.hpp>
#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		bool NeedsRebuild(VkResult result)
		{
			return result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR;
		}
	}

	VkSwapchain::VkSwapchain(VkRenderDevice& device, const Window_T& window) noexcept
		: device(device),
		window(window)
	{
		build();
	}

	VkSwapchain::~VkSwapchain() noexcept
	{
		if (swapchain.swapchain == VK_NULL_HANDLE)
			return;

		vkDeviceWaitIdle(swapchain.device);
		releaseImages();
		vkb::destroy_swapchain(swapchain);
	}

	Format VkSwapchain::getFormat() const
	{
		return FromVkFormat(swapchain.image_format);
	}

	ImageHandle VkSwapchain::acquire(VkSemaphore acquire_semaphore)
	{
		if (window.isMinimized())
			return {};

		if (isOutdated() && !build())
			return {};

		const VkResult result = vkAcquireNextImageKHR(swapchain.device, swapchain, WAIT_FOREVER, acquire_semaphore, VK_NULL_HANDLE, &imageIndex);
		outdated = NeedsRebuild(result);

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			if (result != VK_ERROR_OUT_OF_DATE_KHR)
				DEBUG_ERROR("Failed to acquire swapchain image: {}", string_VkResult(result));

			return {};
		}

		device.resolve(images[imageIndex])->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		return images[imageIndex];
	}

	void VkSwapchain::present(VkQueue present_queue)
	{
		const VkPresentInfoKHR present_info =
		{
			.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores    = &presentSemaphores[imageIndex],
			.swapchainCount     = 1,
			.pSwapchains        = &swapchain.swapchain,
			.pImageIndices      = &imageIndex
		};

		const VkResult result = vkQueuePresentKHR(present_queue, &present_info);
		if (NeedsRebuild(result))
			outdated = true;
		else if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to present swapchain image: {}", string_VkResult(result));
	}

	ImageHandle VkSwapchain::getAcquiredImage() const
	{
		return images[imageIndex];
	}

	VkSemaphore VkSwapchain::getPresentSemaphore() const
	{
		return presentSemaphores[imageIndex];
	}

	uint32_t VkSwapchain::getImageCount() const
	{
		return swapchain.image_count;
	}

	bool VkSwapchain::build()
	{
		vkDeviceWaitIdle(device.getDevice());
		releaseImages();

		requestedExtent = VkExtent2D
		{
			.width  = static_cast<uint32_t>(window.getRenderWidth()),
			.height = static_cast<uint32_t>(window.getRenderHeight())
		};

		auto swapchain_res = vkb::SwapchainBuilder { device.getDevice() }
			.set_desired_extent(requestedExtent.width, requestedExtent.height)
			.set_old_swapchain(swapchain)
			.build();

		if (!swapchain_res)
		{
			DEBUG_ERROR("Failed to create Vulkan swapchain: {}", swapchain_res.error().message());
			return false;
		}

		vkb::destroy_swapchain(swapchain);
		swapchain = swapchain_res.value();

		auto vk_images_res   = swapchain.get_images();
		auto image_views_res = swapchain.get_image_views();
		if (!vk_images_res || !image_views_res)
		{
			DEBUG_ERROR("Failed to retrieve swapchain images");
			return false;
		}

		const std::vector<VkImage> vk_images = vk_images_res.value();
		imageViews = image_views_res.value();

		for (size_t index = 0; index < vk_images.size(); index++)
		{
			images.push_back(device.registerImage({
				.image  = vk_images[index],
				.view   = imageViews[index],
				.format = swapchain.image_format,
				.extent = swapchain.extent
			}));

			VkSemaphore present_semaphore = VK_NULL_HANDLE;
			CreateVkSemaphore(device.getDevice(), VK_SEMAPHORE_TYPE_BINARY, present_semaphore);
			presentSemaphores.push_back(present_semaphore);
		}

		outdated = false;
		DEBUG_LOG("Created swapchain ({}x{}, {} images).", swapchain.extent.width, swapchain.extent.height, swapchain.image_count);
		return true;
	}

	void VkSwapchain::releaseImages()
	{
		for (const ImageHandle image : images)
			device.unregisterImage(image);

		for (const VkSemaphore present_semaphore : presentSemaphores)
			vkDestroySemaphore(device.getDevice(), present_semaphore, nullptr);

		swapchain.destroy_image_views(imageViews);

		images.clear();
		presentSemaphores.clear();
		imageViews.clear();
	}

	bool VkSwapchain::isOutdated() const
	{
		return outdated
			|| requestedExtent.width  != static_cast<uint32_t>(window.getRenderWidth())
			|| requestedExtent.height != static_cast<uint32_t>(window.getRenderHeight());
	}
}
