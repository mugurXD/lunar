#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

#include <cstdint>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr uint64_t WAIT_FOREVER = UINT64_MAX;

		VkResult SubmitCommandBuffer(VkQueue                                queue,
		                             VkCommandBuffer                        command_buffer,
		                             std::span<const VkSemaphoreSubmitInfo> signal_semaphores,
		                             VkFence                                fence)
		{
			const VkCommandBufferSubmitInfo command_buffer_info =
			{
				.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = command_buffer
			};

			const VkSubmitInfo2 submit_info =
			{
				.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
				.commandBufferInfoCount   = 1,
				.pCommandBufferInfos      = &command_buffer_info,
				.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size()),
				.pSignalSemaphoreInfos    = signal_semaphores.data()
			};

			return vkQueueSubmit2(queue, 1, &submit_info, fence);
		}

		void BeginOneTimeCommands(VkCommandBuffer command_buffer)
		{
			const VkCommandBufferBeginInfo begin_info =
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};

			vkResetCommandBuffer(command_buffer, 0);
			vkBeginCommandBuffer(command_buffer, &begin_info);
		}
	}

	bool VkRenderDevice::createUploadResources()
	{
		const VkCommandPoolCreateInfo command_pool_info =
		{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = transferQueueFamilyIndex
		};

		VkResult result = vkCreateCommandPool(device, &command_pool_info, nullptr, &transferCommandPool);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create transfer command pool: {}", string_VkResult(result));
			return false;
		}

		const VkCommandBufferAllocateInfo command_buffer_info =
		{
			.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool        = transferCommandPool,
			.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};

		for (VkUploadBatch& batch : uploadBatches)
		{
			result = vkAllocateCommandBuffers(device, &command_buffer_info, &batch.commandBuffer);
			if (result != VK_SUCCESS)
			{
				DEBUG_ERROR("Failed to allocate upload command buffer: {}", string_VkResult(result));
				return false;
			}
		}

		const VkSemaphoreTypeCreateInfo timeline_info =
		{
			.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue  = 0
		};

		const VkSemaphoreCreateInfo semaphore_info =
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &timeline_info
		};

		result = vkCreateSemaphore(device, &semaphore_info, nullptr, &uploadTimeline);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create upload timeline semaphore: {}", string_VkResult(result));
			return false;
		}

		return true;
	}

	void VkRenderDevice::destroyUploadResources()
	{
		for (VkUploadBatch& batch : uploadBatches)
			releaseStagingBuffers(batch);

		if (uploadTimeline != VK_NULL_HANDLE)
			vkDestroySemaphore(device, uploadTimeline, nullptr);

		if (transferCommandPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(device, transferCommandPool, nullptr);
	}

	UploadTicket VkRenderDevice::flushUploads()
	{
		releaseCompletedBatches();

		VkUploadBatch& batch = currentUploadBatch();
		if (!batch.recording)
			return UploadTicket { nextUploadValue - 1 };

		vkEndCommandBuffer(batch.commandBuffer);

		const VkSemaphoreSubmitInfo signal_info =
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = uploadTimeline,
			.value     = nextUploadValue,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		};

		const VkResult result = SubmitCommandBuffer(transferQueue, batch.commandBuffer, { &signal_info, 1 }, VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
			DEBUG_ERROR("Failed to submit upload batch {}: {}", nextUploadValue, string_VkResult(result));

		batch.signalValue = nextUploadValue++;
		batch.recording   = false;
		return UploadTicket { batch.signalValue };
	}

	bool VkRenderDevice::isComplete(UploadTicket ticket) const
	{
		return getCompletedUploadValue() >= ticket.value;
	}

	VkUploadBatch& VkRenderDevice::currentUploadBatch()
	{
		return uploadBatches[nextUploadValue % UPLOAD_BATCH_COUNT];
	}

	VkUploadBatch& VkRenderDevice::beginUploadBatch()
	{
		VkUploadBatch& batch = currentUploadBatch();
		if (batch.recording)
			return batch;

		waitForUpload(batch.signalValue);
		releaseStagingBuffers(batch);
		BeginOneTimeCommands(batch.commandBuffer);

		batch.recording = true;
		return batch;
	}

	void VkRenderDevice::waitForUpload(uint64_t value)
	{
		if (value >= nextUploadValue)
			flushUploads();

		const VkSemaphoreWaitInfo wait_info =
		{
			.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores    = &uploadTimeline,
			.pValues        = &value
		};

		vkWaitSemaphores(device, &wait_info, WAIT_FOREVER);
	}

	void VkRenderDevice::releaseStagingBuffers(VkUploadBatch& batch)
	{
		for (const VkStagingBuffer& staging : batch.stagingBuffers)
			vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);

		batch.stagingBuffers.clear();
	}

	void VkRenderDevice::releaseCompletedBatches()
	{
		const uint64_t completed_value = getCompletedUploadValue();
		for (VkUploadBatch& batch : uploadBatches)
			if (!batch.recording && batch.signalValue <= completed_value)
				releaseStagingBuffers(batch);
	}

	uint64_t VkRenderDevice::getCompletedUploadValue() const
	{
		uint64_t value = 0;
		vkGetSemaphoreCounterValue(device.device, uploadTimeline, &value);
		return value;
	}

	void VkRenderDevice::submitImmediately(const std::function<void(VkCommandBuffer)>& record_commands)
	{
		BeginOneTimeCommands(mainCommandBuffer);
		record_commands(mainCommandBuffer);
		vkEndCommandBuffer(mainCommandBuffer);

		vkResetFences(device, 1, &immediateFence);

		const VkResult result = SubmitCommandBuffer(graphicsQueue, mainCommandBuffer, {}, immediateFence);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to submit immediate commands: {}", string_VkResult(result));
			return;
		}

		vkWaitForFences(device, 1, &immediateFence, VK_TRUE, WAIT_FOREVER);
	}
}
