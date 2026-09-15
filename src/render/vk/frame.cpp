#include "vk_frame.hpp"
#include "vk_swapchain.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr float VIEWPORT_MIN_DEPTH = 0.f;
		constexpr float VIEWPORT_MAX_DEPTH = 1.f;

		constexpr std::pair<IndexType, VkIndexType> INDEX_TYPE_TRANSLATIONS[] =
		{
			{ IndexType::eUint16, VK_INDEX_TYPE_UINT16 },
			{ IndexType::eUint32, VK_INDEX_TYPE_UINT32 }
		};

		void RecordMemoryBarrier(VkCommandBuffer       command_buffer,
		                         VkPipelineStageFlags2 src_stage_mask,
		                         VkAccessFlags2        src_access_mask,
		                         VkPipelineStageFlags2 dst_stage_mask,
		                         VkAccessFlags2        dst_access_mask)
		{
			const VkMemoryBarrier2 barrier =
			{
				.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
				.srcStageMask  = src_stage_mask,
				.srcAccessMask = src_access_mask,
				.dstStageMask  = dst_stage_mask,
				.dstAccessMask = dst_access_mask
			};

			const VkDependencyInfo dependency =
			{
				.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.memoryBarrierCount = 1,
				.pMemoryBarriers    = &barrier
			};

			vkCmdPipelineBarrier2(command_buffer, &dependency);
		}

		void SetFullViewport(VkCommandBuffer command_buffer, VkExtent2D extent)
		{
			const VkViewport viewport =
			{
				.x        = 0.f,
				.y        = static_cast<float>(extent.height),
				.width    = static_cast<float>(extent.width),
				.height   = -static_cast<float>(extent.height),
				.minDepth = VIEWPORT_MIN_DEPTH,
				.maxDepth = VIEWPORT_MAX_DEPTH
			};

			const VkRect2D scissor = { .extent = extent };

			vkCmdSetViewport(command_buffer, 0, 1, &viewport);
			vkCmdSetScissor(command_buffer, 0, 1, &scissor);
		}

		VkRenderingAttachmentInfo ToVkAttachment(const VkImageRecord& image, LoadOp load_op, const VkClearValue& clear_value)
		{
			return VkRenderingAttachmentInfo
			{
				.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView   = image.view,
				.imageLayout = image.layout,
				.loadOp      = load_op == LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue  = clear_value
			};
		}

		VkClearValue ToClearColor(const glm::vec4& color)
		{
			return VkClearValue { .color = { .float32 = { color.r, color.g, color.b, color.a } } };
		}

		VkClearValue ToClearDepth(float depth)
		{
			return VkClearValue { .depthStencil = { .depth = depth } };
		}
	}

	VkCommandList::VkCommandList(VkRenderDevice& device, uint32_t queue_family_index) noexcept
		: device(device)
	{
		const VkCommandPoolCreateInfo command_pool_info =
		{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = queue_family_index
		};

		VkResult result = vkCreateCommandPool(device.getDevice(), &command_pool_info, nullptr, &commandPool);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create frame command pool: {}", string_VkResult(result));
			return;
		}

		const VkCommandBufferAllocateInfo command_buffer_info =
		{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = commandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		result = vkAllocateCommandBuffers(device.getDevice(), &command_buffer_info, &commandBuffer);
		if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to allocate frame command buffer: {}", string_VkResult(result));
	}

	VkCommandList::~VkCommandList() noexcept
	{
		if (commandPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(device.getDevice(), commandPool, nullptr);
	}

	void VkCommandList::beginRendering(const RenderingDesc& desc)
	{
		std::vector<VkRenderingAttachmentInfo> color_attachments;
		VkExtent2D                             render_extent = {};

		for (const ColorAttachment& attachment : desc.colorAttachments)
		{
			const VkImageRecord& image = prepareAttachment(attachment.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			color_attachments.push_back(ToVkAttachment(image, attachment.loadOp, ToClearColor(attachment.clearColor)));
			render_extent = image.extent;
		}

		std::optional<VkRenderingAttachmentInfo> depth_attachment;
		if (desc.depthAttachment.has_value())
		{
			const VkImageRecord& image = prepareAttachment(desc.depthAttachment->image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			depth_attachment = ToVkAttachment(image, desc.depthAttachment->loadOp, ToClearDepth(desc.depthAttachment->clearDepth));
			render_extent    = image.extent;
		}

		const VkRenderingInfo rendering_info =
		{
			.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea           = { .extent = render_extent },
			.layerCount           = 1,
			.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
			.pColorAttachments    = color_attachments.data(),
			.pDepthAttachment     = depth_attachment.has_value() ? &depth_attachment.value() : nullptr
		};

		vkCmdBeginRendering(commandBuffer, &rendering_info);
		SetFullViewport(commandBuffer, render_extent);
	}

	void VkCommandList::endRendering()
	{
		vkCmdEndRendering(commandBuffer);
	}

	void VkCommandList::bindPipeline(PipelineHandle pipeline)
	{
		const VkPipelineRecord* record = device.resolve(pipeline);
		DEBUG_ASSERT(record != nullptr, "Binding a null or destroyed pipeline");

		vkCmdBindPipeline(commandBuffer, record->bindPoint, record->pipeline);
	}

	void VkCommandList::pushConstants(std::span<const std::byte> data)
	{
		DEBUG_ASSERT(data.size() <= MAX_PUSH_CONSTANTS_SIZE, "Push constants exceed the guaranteed size");
		vkCmdPushConstants(commandBuffer, device.getPipelineLayout(), VK_SHADER_STAGE_ALL, 0, static_cast<uint32_t>(data.size()), data.data());
	}

	void VkCommandList::bindIndexBuffer(BufferHandle buffer, size_t offset, IndexType index_type)
	{
		const VkBufferRecord* record = device.resolve(buffer);
		DEBUG_ASSERT(record != nullptr, "Binding a null or destroyed index buffer");

		vkCmdBindIndexBuffer(commandBuffer, record->buffer, offset, Translate(INDEX_TYPE_TRANSLATIONS, index_type));
	}

	void VkCommandList::memoryBarrier()
	{
		RecordMemoryBarrier(commandBuffer,
		                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		                    VK_ACCESS_2_MEMORY_WRITE_BIT,
		                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		                    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
	}

	void VkCommandList::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
	{
		vkCmdDraw(commandBuffer, vertex_count, instance_count, first_vertex, first_instance);
	}

	void VkCommandList::drawIndexed(uint32_t index_count,
	                                uint32_t instance_count,
	                                uint32_t first_index,
	                                int32_t  vertex_offset,
	                                uint32_t first_instance)
	{
		vkCmdDrawIndexed(commandBuffer, index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void VkCommandList::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
	{
		vkCmdDispatch(commandBuffer, group_count_x, group_count_y, group_count_z);
	}

	void VkCommandList::begin()
	{
		BeginOneTimeCommands(commandBuffer);
	}

	void VkCommandList::end()
	{
		vkEndCommandBuffer(commandBuffer);
	}

	VkImageRecord& VkCommandList::prepareAttachment(ImageHandle image, VkImageLayout layout)
	{
		VkImageRecord* record = device.resolve(image);
		DEBUG_ASSERT(record != nullptr, "Rendering to a null or destroyed image");

		transitionImage(*record, layout);
		return *record;
	}

	void VkCommandList::transitionImage(VkImageRecord& image, VkImageLayout layout)
	{
		const VkImageMemoryBarrier2 barrier =
		{
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask       = VK_ACCESS_2_MEMORY_WRITE_BIT,
			.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
			.oldLayout           = image.layout,
			.newLayout           = layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = image.image,
			.subresourceRange    =
			{
				.aspectMask = image.aspect,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS
			}
		};

		const VkDependencyInfo dependency =
		{
			.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &barrier
		};

		vkCmdPipelineBarrier2(commandBuffer, &dependency);
		image.layout = layout;
	}

	VkCommandBuffer VkCommandList::getHandle() const
	{
		return commandBuffer;
	}

	VkFrame::VkFrame(VkRenderDevice& device, uint32_t queue_family_index) noexcept
		: device(device),
		commands(device, queue_family_index)
	{
		const VkResult result = CreateVkSemaphore(device.getDevice(), VK_SEMAPHORE_TYPE_BINARY, acquireSemaphore);
		if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to create frame acquire semaphore: {}", string_VkResult(result));
	}

	VkFrame::~VkFrame() noexcept
	{
		if (acquireSemaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(device.getDevice(), acquireSemaphore, nullptr);
	}

	ImageHandle VkFrame::acquire(Swapchain& swapchain)
	{
		DEBUG_ASSERT(acquiredSwapchain == nullptr, "A frame can only acquire one swapchain image");

		VkSwapchain&      vk_swapchain = static_cast<VkSwapchain&>(swapchain);
		const ImageHandle image        = vk_swapchain.acquire(acquireSemaphore);

		if (image != ImageHandle {})
			acquiredSwapchain = &vk_swapchain;

		return image;
	}

	CommandList& VkFrame::commandList()
	{
		return commands;
	}

	bool VkRenderDevice::createFrameResources()
	{
		const VkResult result = CreateVkSemaphore(device, VK_SEMAPHORE_TYPE_TIMELINE, frameTimeline);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create frame timeline semaphore: {}", string_VkResult(result));
			return false;
		}

		for (std::unique_ptr<VkFrame>& frame : frames)
			frame = std::make_unique<VkFrame>(*this, graphicsQueueFamilyIndex);

		return true;
	}

	void VkRenderDevice::destroyFrameResources()
	{
		for (std::unique_ptr<VkFrame>& frame : frames)
			frame.reset();

		if (frameTimeline != VK_NULL_HANDLE)
			vkDestroySemaphore(device, frameTimeline, nullptr);
	}

	void VkRenderDevice::destroyLater(UploadTicket pending_upload, std::function<void()> destroy)
	{
		deferredDestructions.push_back({
			.destroy     = std::move(destroy),
			.frameValue  = frameValue,
			.uploadValue = pending_upload.value
		});

		releaseDestroyedResources();
	}

	void VkRenderDevice::releaseDestroyedResources()
	{
		const uint64_t completed_frame  = GetTimelineValue(device, frameTimeline);
		const uint64_t completed_upload = getCompletedUploadValue();

		std::erase_if(deferredDestructions, [&](const VkDeferredDestruction& destruction) {
			const bool released = destruction.frameValue <= completed_frame && destruction.uploadValue <= completed_upload;
			if (released)
				destruction.destroy();

			return released;
		});
	}

	Frame& VkRenderDevice::beginFrame()
	{
		VkFrame& frame = *frames[(frameValue + 1) % FRAMES_IN_FLIGHT];
		WaitForTimeline(device, frameTimeline, frame.signalValue);

		frame.signalValue = ++frameValue;
		releaseDestroyedResources();
		frame.commands.begin();

		return frame;
	}

	void VkRenderDevice::endFrame(Frame& base_frame)
	{
		VkFrame&           frame     = static_cast<VkFrame&>(base_frame);
		VkSwapchain* const swapchain = frame.acquiredSwapchain;
		const UploadTicket uploads   = flushUploads();

		std::vector<VkSemaphoreSubmitInfo> wait_semaphores   = { MakeSemaphoreSubmitInfo(uploadTimeline, uploads.value, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) };
		std::vector<VkSemaphoreSubmitInfo> signal_semaphores = { MakeSemaphoreSubmitInfo(frameTimeline, frame.signalValue, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT) };

		if (swapchain != nullptr)
		{
			frame.commands.transitionImage(*resolve(swapchain->getAcquiredImage()), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			wait_semaphores.push_back(MakeSemaphoreSubmitInfo(frame.acquireSemaphore, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT));
			signal_semaphores.push_back(MakeSemaphoreSubmitInfo(swapchain->getPresentSemaphore(), 0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT));
		}

		RecordMemoryBarrier(frame.commands.getHandle(),
		                    VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		                    VK_ACCESS_2_MEMORY_WRITE_BIT,
		                    VK_PIPELINE_STAGE_2_HOST_BIT,
		                    VK_ACCESS_2_HOST_READ_BIT);

		frame.commands.end();

		const VkResult result = SubmitCommandBuffer(graphicsQueue, frame.commands.getHandle(), wait_semaphores, signal_semaphores, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to submit frame {}: {}", frame.signalValue, string_VkResult(result));

		if (swapchain != nullptr)
			swapchain->present(presentQueue);

		frame.acquiredSwapchain = nullptr;
	}
}
