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

		void beginRendering(const RenderingDesc& desc)                                  override;
		void endRendering()                                                             override;
		void bindPipeline(PipelineHandle pipeline)                                      override;
		void pushConstants(std::span<const std::byte> data)                             override;
		void bindIndexBuffer(BufferHandle buffer, size_t offset, IndexType index_type) override;
		void memoryBarrier()                                                            override;
		void draw(uint32_t vertex_count,
		          uint32_t instance_count,
		          uint32_t first_vertex,
		          uint32_t first_instance)                                              override;
		void drawIndexed(uint32_t index_count,
		                 uint32_t instance_count,
		                 uint32_t first_index,
		                 int32_t  vertex_offset,
		                 uint32_t first_instance)                                       override;
		void dispatch(uint32_t group_count_x,
		              uint32_t group_count_y,
		              uint32_t group_count_z)                                           override;

		using CommandList::pushConstants;

		void            begin();
		void            end();
		void            transitionImage(VkImageRecord& image, VkImageLayout layout);
		VkCommandBuffer getHandle() const;

	private:
		VkImageRecord& prepareAttachment(ImageHandle image, VkImageLayout layout);

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
