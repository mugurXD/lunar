#pragma once
#include "vk_render_device.hpp"

namespace lunar::Render::imp
{
	class VkSwapchain;

	class VkCommandList final : public CommandList
	{
	public:
		VkCommandList(VkRenderDevice& device, uint32_t queue_family_index) noexcept;
		~VkCommandList() noexcept override;

		void beginRendering(const RenderingDesc& desc) override;
		void endRendering()                            override;

		void            begin();
		void            end();
		void            transitionImage(VkImageRecord& image, VkImageLayout layout);
		VkCommandBuffer getHandle() const;

	private:
		VkRenderDevice& device;
		VkCommandPool   commandPool   = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	};

	class VkFrame final : public Frame
	{
	public:
		VkFrame(VkRenderDevice& device, uint32_t queue_family_index) noexcept;
		~VkFrame() noexcept override;

		ImageHandle  acquire(Swapchain& swapchain) override;
		CommandList& commandList()                 override;

	private:
		VkRenderDevice& device;
		VkCommandList   commands;
		VkSemaphore     acquireSemaphore  = VK_NULL_HANDLE;
		VkSwapchain*    acquiredSwapchain = nullptr;
		uint64_t        signalValue       = 0;

		friend class VkRenderDevice;
	};
}
