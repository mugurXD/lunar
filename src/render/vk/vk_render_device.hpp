#pragma once
#include <lunar/render/render_device.hpp>
#include <lunar/core/handle.hpp>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <VkBootstrap.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace lunar::Render::imp
{
	class VkFrame;

	inline constexpr uint64_t WAIT_FOREVER = UINT64_MAX;

	struct VkBufferAllocation
	{
		VkBuffer      buffer     = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
	};

	struct VkBufferRecord
	{
		VkBuffer        buffer          = VK_NULL_HANDLE;
		VmaAllocation   allocation      = VK_NULL_HANDLE;
		void*           mapped          = nullptr;
		VkDeviceAddress address         = 0;
		size_t          size            = 0;
		uint64_t        lastUploadValue = 0;
	};

	struct VkDestroyedBuffer
	{
		VkBufferAllocation buffer      = {};
		uint64_t           frameValue  = 0;
		uint64_t           uploadValue = 0;
	};

	struct VkImageRecord
	{
		VkImage       image  = VK_NULL_HANDLE;
		VkImageView   view   = VK_NULL_HANDLE;
		VkFormat      format = VK_FORMAT_UNDEFINED;
		VkExtent2D    extent = {};
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	struct VkUploadBatch
	{
		VkCommandBuffer                 commandBuffer  = VK_NULL_HANDLE;
		uint64_t                        signalValue    = 0;
		bool                            recording      = false;
		std::vector<VkBufferAllocation> stagingBuffers = {};
	};

	template<typename Tag, typename T>
	GpuHandle<Tag> ToGpuHandle(const PoolHandle<T>& handle)
	{
		return { handle.getIndex(), handle.getGeneration() };
	}

	template<typename T, typename Tag>
	PoolHandle<T> FromGpuHandle(Pool<T>& pool, GpuHandle<Tag> handle)
	{
		return pool.getHandleFor(handle.index, handle.generation);
	}

	VkResult              SubmitCommandBuffer(VkQueue                                queue,
	                                          VkCommandBuffer                        command_buffer,
	                                          std::span<const VkSemaphoreSubmitInfo> wait_semaphores,
	                                          std::span<const VkSemaphoreSubmitInfo> signal_semaphores,
	                                          VkFence                                fence);
	VkSemaphoreSubmitInfo MakeSemaphoreSubmitInfo(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 stage_mask);
	void                  BeginOneTimeCommands(VkCommandBuffer command_buffer);
	VkResult              CreateVkSemaphore(VkDevice device, VkSemaphoreType type, VkSemaphore& semaphore);
	uint64_t              GetTimelineValue(VkDevice device, VkSemaphore timeline);
	void                  WaitForTimeline(VkDevice device, VkSemaphore timeline, uint64_t value);

	class VkRenderDevice final : public RenderDevice
	{
	public:
		VkRenderDevice(const RenderDeviceSettings& settings) noexcept;
		~VkRenderDevice() noexcept override;

		std::unique_ptr<Swapchain> createSwapchain(Window_T& window)                                                  override;
		Frame&                     beginFrame()                                                                       override;
		void                       endFrame(Frame& frame)                                                             override;
		BufferHandle               createBuffer(const BufferDesc& desc, std::span<const std::byte> initial_data)      override;
		void                       destroyBuffer(BufferHandle buffer)                                                 override;
		UploadTicket               uploadBuffer(BufferHandle buffer, size_t offset, std::span<const std::byte> data) override;
		UploadTicket               flushUploads()                                                                     override;
		bool                       isComplete(UploadTicket ticket)                                              const override;
		uint64_t                   getBufferAddress(BufferHandle buffer)                                              override;

		const vkb::Device& getDevice() const;
		VkBufferRecord*    resolve(BufferHandle buffer);
		VkImageRecord*     resolve(ImageHandle image);
		ImageHandle        registerImage(const VkImageRecord& record);
		void               unregisterImage(ImageHandle image);

	private:
		static constexpr size_t UPLOAD_BATCH_COUNT = 3;
		static constexpr size_t FRAMES_IN_FLIGHT   = 2;

		UploadTicket    uploadThroughStaging(VkBufferRecord& record, size_t offset, std::span<const std::byte> data);
		void            submitImmediately(const std::function<void(VkCommandBuffer)>& record_commands);
		void            releaseDestroyedBuffers();

		bool            createFrameResources();
		void            destroyFrameResources();

		bool            createUploadResources();
		void            destroyUploadResources();
		VkUploadBatch&  currentUploadBatch();
		VkUploadBatch&  beginUploadBatch();
		void            waitForUpload(uint64_t value);
		void            releaseStagingBuffers(VkUploadBatch& batch);
		void            releaseCompletedBatches();
		uint64_t        getCompletedUploadValue() const;

		Window_T*                                              presentWindow            = nullptr;
		vkb::Instance                                          instance                 = {};
		VkSurfaceKHR                                           surface                  = VK_NULL_HANDLE;
		vkb::Device                                            device                   = {};
		VkQueue                                                graphicsQueue            = VK_NULL_HANDLE;
		VkQueue                                                presentQueue             = VK_NULL_HANDLE;
		VkQueue                                                transferQueue            = VK_NULL_HANDLE;
		uint32_t                                               graphicsQueueFamilyIndex = 0;
		uint32_t                                               presentQueueFamilyIndex  = 0;
		uint32_t                                               transferQueueFamilyIndex = 0;
		VkCommandPool                                          commandPool              = VK_NULL_HANDLE;
		VkCommandBuffer                                        mainCommandBuffer        = VK_NULL_HANDLE;
		VkFence                                                immediateFence           = VK_NULL_HANDLE;
		VmaAllocator                                           allocator                = VK_NULL_HANDLE;
		VkSemaphore                                            frameTimeline            = VK_NULL_HANDLE;
		uint64_t                                               frameValue               = 0;
		std::array<std::unique_ptr<VkFrame>, FRAMES_IN_FLIGHT> frames;
		VkCommandPool                                          transferCommandPool      = VK_NULL_HANDLE;
		VkSemaphore                                            uploadTimeline           = VK_NULL_HANDLE;
		uint64_t                                               nextUploadValue          = 1;
		std::array<VkUploadBatch, UPLOAD_BATCH_COUNT>          uploadBatches            = {};
		std::vector<VkDestroyedBuffer>                         destroyedBuffers         = {};
		Pool<VkBufferRecord>                                   buffers;
		Pool<VkImageRecord>                                    images;
	};
}
