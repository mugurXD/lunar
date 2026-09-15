#pragma once
#include <lunar/render/render_device.hpp>
#include <lunar/core/handle.hpp>

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <VkBootstrap.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <utility>
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

	struct VkDeferredDestruction
	{
		std::function<void()> destroy     = {};
		uint64_t              frameValue  = 0;
		uint64_t              uploadValue = 0;
	};

	struct VkPipelineRecord
	{
		VkPipeline          pipeline  = VK_NULL_HANDLE;
		VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	};

	struct VkImageRecord
	{
		VkImage            image      = VK_NULL_HANDLE;
		VkImageView        view       = VK_NULL_HANDLE;
		VmaAllocation      allocation = VK_NULL_HANDLE;
		VkFormat           format     = VK_FORMAT_UNDEFINED;
		VkExtent2D         extent     = {};
		VkImageAspectFlags aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageLayout      layout     = VK_IMAGE_LAYOUT_UNDEFINED;
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

	template<typename From, typename To, size_t Count>
	constexpr To Translate(const std::pair<From, To> (&table)[Count], From value)
	{
		const auto found = std::ranges::find(table, value, &std::pair<From, To>::first);
		return found != std::end(table) ? found->second : To {};
	}

	template<typename From, typename To, size_t Count>
	constexpr From TranslateBack(const std::pair<From, To> (&table)[Count], To value)
	{
		const auto found = std::ranges::find(table, value, &std::pair<From, To>::second);
		return found != std::end(table) ? found->first : From {};
	}

	template<typename BitType, typename VkBitType, size_t Count>
	VkFlags TranslateFlags(const std::pair<BitType, VkBitType> (&table)[Count], Flags<BitType> flags)
	{
		VkFlags vk_flags = 0;
		for (const auto& [bit, vk_bit] : table)
			if (flags & bit)
				vk_flags |= vk_bit;

		return vk_flags;
	}

	VkFormat ToVkFormat(Format format);
	Format   FromVkFormat(VkFormat format);

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
		void                       waitIdle()                                                                         override;
		RenderDeviceStats          getStats()                                                                   const override;
		BufferHandle               createBuffer(const BufferDesc& desc, std::span<const std::byte> initial_data)      override;
		void                       destroyBuffer(BufferHandle buffer)                                                 override;
		UploadTicket               uploadBuffer(BufferHandle buffer, size_t offset, std::span<const std::byte> data) override;
		UploadTicket               flushUploads()                                                                     override;
		bool                       isComplete(UploadTicket ticket)                                              const override;
		uint64_t                   getBufferAddress(BufferHandle buffer)                                              override;
		std::span<const std::byte> readBuffer(BufferHandle buffer)                                                    override;
		ImageHandle                createImage(const ImageDesc& desc)                                                 override;
		void                       destroyImage(ImageHandle image)                                                    override;
		Extent2D                   getImageExtent(ImageHandle image)                                                  override;
		PipelineHandle             createGraphicsPipeline(const GraphicsPipelineDesc& desc)                           override;
		PipelineHandle             createComputePipeline(const ComputePipelineDesc& desc)                             override;
		void                       destroyPipeline(PipelineHandle pipeline)                                           override;

		const vkb::Device& getDevice()         const;
		VkPipelineLayout   getPipelineLayout() const;
		VkBufferRecord*    resolve(BufferHandle buffer);
		VkImageRecord*     resolve(ImageHandle image);
		VkPipelineRecord*  resolve(PipelineHandle pipeline);
		ImageHandle        registerImage(const VkImageRecord& record);
		void               unregisterImage(ImageHandle image);

	private:
		static constexpr size_t UPLOAD_BATCH_COUNT = 3;
		static constexpr size_t FRAMES_IN_FLIGHT   = 2;

		UploadTicket    uploadThroughStaging(VkBufferRecord& record, size_t offset, std::span<const std::byte> data);
		void            submitImmediately(const std::function<void(VkCommandBuffer)>& record_commands);

		bool            createPipelineLayout();
		PipelineHandle  registerPipeline(VkResult result, VkPipeline pipeline, VkPipelineBindPoint bind_point);

		bool            createFrameResources();
		void            destroyFrameResources();
		void            destroyLater(UploadTicket pending_upload, std::function<void()> destroy);
		void            releaseDestroyedResources();

		bool            createUploadResources();
		void            destroyUploadResources();
		VkUploadBatch&  currentUploadBatch();
		VkUploadBatch&  beginUploadBatch();
		void            waitForUpload(uint64_t value);
		void            releaseStagingBuffers(VkUploadBatch& batch);
		void            releaseCompletedBatches();
		uint64_t        getCompletedUploadValue() const;

		std::atomic<uint64_t>                                  validationErrorCount     = 0;
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
		VkPipelineLayout                                       pipelineLayout           = VK_NULL_HANDLE;
		VkSemaphore                                            frameTimeline            = VK_NULL_HANDLE;
		uint64_t                                               frameValue               = 0;
		std::array<std::unique_ptr<VkFrame>, FRAMES_IN_FLIGHT> frames;
		VkCommandPool                                          transferCommandPool      = VK_NULL_HANDLE;
		VkSemaphore                                            uploadTimeline           = VK_NULL_HANDLE;
		uint64_t                                               nextUploadValue          = 1;
		std::array<VkUploadBatch, UPLOAD_BATCH_COUNT>          uploadBatches            = {};
		std::vector<VkDeferredDestruction>                     deferredDestructions     = {};
		Pool<VkBufferRecord>                                   buffers;
		Pool<VkImageRecord>                                    images;
		Pool<VkPipelineRecord>                                 pipelines;
	};
}
