#include "vk_frame.hpp"
#include "vk_swapchain.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

namespace lunar::Render::imp
{
	namespace
	{
		VkRenderingAttachmentInfo ToVkAttachment(const ColorAttachment& attachment, const VkImageRecord& image)
		{
			const glm::vec4& color = attachment.clearColor;

			return VkRenderingAttachmentInfo
			{
				.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView   = image.view,
				.imageLayout = image.layout,
				.loadOp      = attachment.loadOp == LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue  = { .color = { .float32 = { color.r, color.g, color.b, color.a } } }
			};
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
			VkImageRecord* image = device.resolve(attachment.image);
			DEBUG_ASSERT(image != nullptr, "Rendering to a null or destroyed image");

			transitionImage(*image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			color_attachments.push_back(ToVkAttachment(attachment, *image));
			render_extent = image->extent;
		}

		const VkRenderingInfo rendering_info =
		{
			.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea           = { .extent = render_extent },
			.layerCount           = 1,
			.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
			.pColorAttachments    = color_attachments.data()
		};

		vkCmdBeginRendering(commandBuffer, &rendering_info);
	}

	void VkCommandList::endRendering()
	{
		vkCmdEndRendering(commandBuffer);
	}

	void VkCommandList::begin()
	{
		BeginOneTimeCommands(commandBuffer);
	}

	void VkCommandList::end()
	{
		vkEndCommandBuffer(commandBuffer);
	}

	void VkCommandList::transitionImage(VkImageRecord& image, VkImageLayout layout)
	{
		if (image.layout == layout)
			return;

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
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
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

	Frame& VkRenderDevice::beginFrame()
	{
		VkFrame& frame = *frames[(frameValue + 1) % FRAMES_IN_FLIGHT];
		WaitForTimeline(device, frameTimeline, frame.signalValue);

		frame.signalValue = ++frameValue;
		releaseDestroyedBuffers();
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

		frame.commands.end();

		const VkResult result = SubmitCommandBuffer(graphicsQueue, frame.commands.getHandle(), wait_semaphores, signal_semaphores, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to submit frame {}: {}", frame.signalValue, string_VkResult(result));

		if (swapchain != nullptr)
			swapchain->present(presentQueue);

		frame.acquiredSwapchain = nullptr;
	}
}
