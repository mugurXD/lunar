#pragma once
#include <lunar/render/render_device.hpp>
#include <lunar/core/handle.hpp>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <VkBootstrap.h>

#include <array>
#include <functional>
#include <vector>

namespace lunar::Render::imp
{
	struct VkBufferRecord
	{
		VkBuffer        buffer          = VK_NULL_HANDLE;
		VmaAllocation   allocation      = VK_NULL_HANDLE;
		void*           mapped          = nullptr;
		VkDeviceAddress address         = 0;
		size_t          size            = 0;
		uint64_t        lastUploadValue = 0;
	};

	struct VkStagingBuffer
	{
		VkBuffer      buffer     = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
	};

	struct VkUploadBatch
	{
		VkCommandBuffer              commandBuffer  = VK_NULL_HANDLE;
		uint64_t                     signalValue    = 0;
		bool                         recording      = false;
		std::vector<VkStagingBuffer> stagingBuffers = {};
	};

	class VkRenderDevice final : public RenderDevice
	{
	public:
		VkRenderDevice(const RenderDeviceSettings& settings) noexcept;
		~VkRenderDevice() noexcept override;

		std::unique_ptr<Swapchain> createSwapchain(Window_T& window)                                                  override;
		BufferHandle               createBuffer(const BufferDesc& desc, std::span<const std::byte> initial_data)      override;
		void                       destroyBuffer(BufferHandle buffer)                                                 override;
		UploadTicket               uploadBuffer(BufferHandle buffer, size_t offset, std::span<const std::byte> data) override;
		UploadTicket               flushUploads()                                                                     override;
		bool                       isComplete(UploadTicket ticket)                                              const override;
		uint64_t                   getBufferAddress(BufferHandle buffer)                                              override;

	private:
		static constexpr size_t UPLOAD_BATCH_COUNT = 3;

		VkBufferRecord* resolve(BufferHandle buffer);
		UploadTicket    uploadThroughStaging(VkBufferRecord& record, size_t offset, std::span<const std::byte> data);
		void            submitImmediately(const std::function<void(VkCommandBuffer)>& record_commands);

		bool            createUploadResources();
		void            destroyUploadResources();
		VkUploadBatch&  currentUploadBatch();
		VkUploadBatch&  beginUploadBatch();
		void            waitForUpload(uint64_t value);
		void            releaseStagingBuffers(VkUploadBatch& batch);
		void            releaseCompletedBatches();
		uint64_t        getCompletedUploadValue() const;

		Window_T*                                     presentWindow            = nullptr;
		vkb::Instance                                 instance                 = {};
		VkSurfaceKHR                                  surface                  = VK_NULL_HANDLE;
		vkb::Device                                   device                   = {};
		VkQueue                                       graphicsQueue            = VK_NULL_HANDLE;
		VkQueue                                       presentQueue             = VK_NULL_HANDLE;
		VkQueue                                       transferQueue            = VK_NULL_HANDLE;
		uint32_t                                      graphicsQueueFamilyIndex = 0;
		uint32_t                                      presentQueueFamilyIndex  = 0;
		uint32_t                                      transferQueueFamilyIndex = 0;
		VkCommandPool                                 commandPool              = VK_NULL_HANDLE;
		VkCommandBuffer                               mainCommandBuffer        = VK_NULL_HANDLE;
		VkFence                                       immediateFence           = VK_NULL_HANDLE;
		VmaAllocator                                  allocator                = VK_NULL_HANDLE;
		VkCommandPool                                 transferCommandPool      = VK_NULL_HANDLE;
		VkSemaphore                                   uploadTimeline           = VK_NULL_HANDLE;
		uint64_t                                      nextUploadValue          = 1;
		std::array<VkUploadBatch, UPLOAD_BATCH_COUNT> uploadBatches            = {};
		Pool<VkBufferRecord>                          buffers;
	};
}
