#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

#include <cstdint>
#include <cstring>
#include <utility>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr std::pair<BufferUsageFlagBits, VkBufferUsageFlagBits> USAGE_TRANSLATIONS[] =
		{
			{ BufferUsageFlagBits::eVertex,      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT   },
			{ BufferUsageFlagBits::eIndex,       VK_BUFFER_USAGE_INDEX_BUFFER_BIT    },
			{ BufferUsageFlagBits::eUniform,     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT  },
			{ BufferUsageFlagBits::eStorage,     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT  },
			{ BufferUsageFlagBits::eIndirect,    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT },
			{ BufferUsageFlagBits::eTransferSrc, VK_BUFFER_USAGE_TRANSFER_SRC_BIT    },
			{ BufferUsageFlagBits::eTransferDst, VK_BUFFER_USAGE_TRANSFER_DST_BIT    },
		};

		bool IsAddressable(BufferUsageFlags usage)
		{
			return (usage & BufferUsageFlagBits::eVertex)
				|| (usage & BufferUsageFlagBits::eIndex)
				|| (usage & BufferUsageFlagBits::eStorage);
		}

		VkBufferUsageFlags ToVkBufferUsage(BufferUsageFlags usage, MemoryLocation location)
		{
			VkBufferUsageFlags vk_usage = 0;
			for (const auto& [bit, vk_bit] : USAGE_TRANSLATIONS)
				if (usage & bit)
					vk_usage |= vk_bit;

			if (IsAddressable(usage))
				vk_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

			if (location == MemoryLocation::eGpuOnly)
				vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

			return vk_usage;
		}

		VmaAllocationCreateFlags ToVmaAllocationFlags(MemoryLocation location)
		{
			switch (location)
			{
			case MemoryLocation::eUpload:
				return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			case MemoryLocation::eReadback:
				return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			default:
				return 0;
			}
		}

		void SerializeCopies(VkCommandBuffer command_buffer)
		{
			const VkMemoryBarrier2 barrier =
			{
				.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
				.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
				.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
				.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT
			};

			const VkDependencyInfo dependency =
			{
				.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.memoryBarrierCount = 1,
				.pMemoryBarriers    = &barrier
			};

			vkCmdPipelineBarrier2(command_buffer, &dependency);
		}
	}

	BufferHandle VkRenderDevice::createBuffer(const BufferDesc& desc, std::span<const std::byte> initial_data)
	{
		const VkBufferUsageFlags      usage                 = ToVkBufferUsage(desc.usage, desc.location);
		const std::array<uint32_t, 2> upload_queue_families = { graphicsQueueFamilyIndex, transferQueueFamilyIndex };
		const bool                    shared_with_transfer  = desc.location == MemoryLocation::eGpuOnly
			&& graphicsQueueFamilyIndex != transferQueueFamilyIndex;

		const VkBufferCreateInfo buffer_info =
		{
			.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size                  = desc.size,
			.usage                 = usage,
			.sharingMode           = shared_with_transfer ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = shared_with_transfer ? static_cast<uint32_t>(upload_queue_families.size()) : 0,
			.pQueueFamilyIndices   = upload_queue_families.data()
		};

		const VmaAllocationCreateInfo allocation_info =
		{
			.flags = ToVmaAllocationFlags(desc.location),
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VkBufferRecord    record            = { .size = desc.size };
		VmaAllocationInfo allocation_result = {};

		const VkResult result = vmaCreateBuffer(allocator, &buffer_info, &allocation_info, &record.buffer, &record.allocation, &allocation_result);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create buffer of {} bytes: {}", desc.size, string_VkResult(result));
			return {};
		}

		record.mapped = allocation_result.pMappedData;

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			const VkBufferDeviceAddressInfo address_info =
			{
				.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
				.buffer = record.buffer
			};

			record.address = vkGetBufferDeviceAddress(device, &address_info);
		}

		const PoolHandle<VkBufferRecord> stored = buffers.create(record);
		const BufferHandle               handle = { stored.getIndex(), stored.getGeneration() };

		if (!initial_data.empty())
			uploadBuffer(handle, 0, initial_data);

		return handle;
	}

	void VkRenderDevice::destroyBuffer(BufferHandle buffer)
	{
		const PoolHandle<VkBufferRecord> stored = buffers.getHandleFor(buffer.index, buffer.generation);
		const VkBufferRecord*            record = buffers.get(stored);
		if (record == nullptr)
			return;

		waitForUpload(record->lastUploadValue);
		vmaDestroyBuffer(allocator, record->buffer, record->allocation);
		buffers.destroy(stored);
	}

	UploadTicket VkRenderDevice::uploadBuffer(BufferHandle buffer, size_t offset, std::span<const std::byte> data)
	{
		VkBufferRecord* record = resolve(buffer);
		DEBUG_ASSERT(record != nullptr, "Uploading to a null or destroyed buffer");
		DEBUG_ASSERT(offset + data.size() <= record->size, "Upload does not fit inside the buffer");

		if (record->mapped == nullptr)
			return uploadThroughStaging(*record, offset, data);

		std::memcpy(static_cast<std::byte*>(record->mapped) + offset, data.data(), data.size());
		vmaFlushAllocation(allocator, record->allocation, offset, data.size());
		return UploadTicket {};
	}

	uint64_t VkRenderDevice::getBufferAddress(BufferHandle buffer)
	{
		const VkBufferRecord* record = resolve(buffer);
		return record == nullptr ? 0 : record->address;
	}

	VkBufferRecord* VkRenderDevice::resolve(BufferHandle buffer)
	{
		return buffers.get(buffers.getHandleFor(buffer.index, buffer.generation));
	}

	UploadTicket VkRenderDevice::uploadThroughStaging(VkBufferRecord& record, size_t offset, std::span<const std::byte> data)
	{
		const VkBufferCreateInfo staging_info =
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size  = data.size(),
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		};

		const VmaAllocationCreateInfo staging_allocation_info =
		{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VkStagingBuffer   staging        = {};
		VmaAllocationInfo staging_result = {};

		const VkResult result = vmaCreateBuffer(allocator, &staging_info, &staging_allocation_info, &staging.buffer, &staging.allocation, &staging_result);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create staging buffer of {} bytes: {}", data.size(), string_VkResult(result));
			return UploadTicket {};
		}

		std::memcpy(staging_result.pMappedData, data.data(), data.size());
		vmaFlushAllocation(allocator, staging.allocation, 0, data.size());

		VkUploadBatch& batch = beginUploadBatch();

		if (record.lastUploadValue == nextUploadValue)
			SerializeCopies(batch.commandBuffer);

		const VkBufferCopy region =
		{
			.srcOffset = 0,
			.dstOffset = offset,
			.size      = data.size()
		};

		vkCmdCopyBuffer(batch.commandBuffer, staging.buffer, record.buffer, 1, &region);
		batch.stagingBuffers.push_back(staging);

		record.lastUploadValue = nextUploadValue;
		return UploadTicket { nextUploadValue };
	}
}
