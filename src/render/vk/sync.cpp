#include "vk_render_device.hpp"

namespace lunar::Render::imp
{
	VkResult SubmitCommandBuffer(VkQueue                                queue,
	                             VkCommandBuffer                        command_buffer,
	                             std::span<const VkSemaphoreSubmitInfo> wait_semaphores,
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
			.waitSemaphoreInfoCount   = static_cast<uint32_t>(wait_semaphores.size()),
			.pWaitSemaphoreInfos      = wait_semaphores.data(),
			.commandBufferInfoCount   = 1,
			.pCommandBufferInfos      = &command_buffer_info,
			.signalSemaphoreInfoCount = static_cast<uint32_t>(signal_semaphores.size()),
			.pSignalSemaphoreInfos    = signal_semaphores.data()
		};

		return vkQueueSubmit2(queue, 1, &submit_info, fence);
	}

	VkSemaphoreSubmitInfo MakeSemaphoreSubmitInfo(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 stage_mask)
	{
		return VkSemaphoreSubmitInfo
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = semaphore,
			.value     = value,
			.stageMask = stage_mask
		};
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

	VkResult CreateVkSemaphore(VkDevice device, VkSemaphoreType type, VkSemaphore& semaphore)
	{
		const VkSemaphoreTypeCreateInfo type_info =
		{
			.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = type,
			.initialValue  = 0
		};

		const VkSemaphoreCreateInfo semaphore_info =
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &type_info
		};

		return vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
	}

	uint64_t GetTimelineValue(VkDevice device, VkSemaphore timeline)
	{
		uint64_t value = 0;
		vkGetSemaphoreCounterValue(device, timeline, &value);
		return value;
	}

	void WaitForTimeline(VkDevice device, VkSemaphore timeline, uint64_t value)
	{
		const VkSemaphoreWaitInfo wait_info =
		{
			.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores    = &timeline,
			.pValues        = &value
		};

		vkWaitSemaphores(device, &wait_info, WAIT_FOREVER);
	}
}
