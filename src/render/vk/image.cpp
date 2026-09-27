#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

#include <bit>

namespace lunar::Render::imp
{
	namespace
	{
		constexpr uint32_t IMAGE_2D_DEPTH = 1;

		constexpr std::pair<Format, VkFormat> FORMAT_TRANSLATIONS[] =
		{
			{ Format::eUndefined,   VK_FORMAT_UNDEFINED           },
			{ Format::eRGBA8Unorm,  VK_FORMAT_R8G8B8A8_UNORM      },
			{ Format::eRGBA8Srgb,   VK_FORMAT_R8G8B8A8_SRGB       },
			{ Format::eBGRA8Unorm,  VK_FORMAT_B8G8R8A8_UNORM      },
			{ Format::eBGRA8Srgb,   VK_FORMAT_B8G8R8A8_SRGB       },
			{ Format::eRGBA16Float, VK_FORMAT_R16G16B16A16_SFLOAT },
			{ Format::eRGBA32Float, VK_FORMAT_R32G32B32A32_SFLOAT },
			{ Format::eD32Float,    VK_FORMAT_D32_SFLOAT          }
		};

		constexpr std::pair<ImageUsageFlagBits, VkImageUsageFlagBits> USAGE_TRANSLATIONS[] =
		{
			{ ImageUsageFlagBits::eColorAttachment, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         },
			{ ImageUsageFlagBits::eDepthAttachment, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT },
			{ ImageUsageFlagBits::eSampled,         VK_IMAGE_USAGE_SAMPLED_BIT                  },
			{ ImageUsageFlagBits::eStorage,         VK_IMAGE_USAGE_STORAGE_BIT                  },
			{ ImageUsageFlagBits::eTransferSrc,     VK_IMAGE_USAGE_TRANSFER_SRC_BIT             },
			{ ImageUsageFlagBits::eTransferDst,     VK_IMAGE_USAGE_TRANSFER_DST_BIT             }
		};

		VkImageAspectFlags AspectOf(Format format)
		{
			return format == Format::eD32Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		}

		size_t BytesPerPixel(Format format)
		{
			switch (format)
			{
			case Format::eRGBA16Float: return 8;
			case Format::eRGBA32Float: return 16;
			case Format::eUndefined:   return 0;
			default:                   return 4;
			}
		}

		bool Contains(const VkExtent2D& extent, const Rect2D& region)
		{
			return region.offset.x >= 0 && region.offset.y >= 0 && region.extent.width > 0 && region.extent.height > 0
				&& static_cast<uint64_t>(region.offset.x) + region.extent.width  <= extent.width
				&& static_cast<uint64_t>(region.offset.y) + region.extent.height <= extent.height;
		}

		bool IsValid(const ImageDesc& desc)
		{
			return desc.extent.width > 0
				&& desc.extent.height > 0
				&& desc.format != Format::eUndefined
				&& desc.mipLevels > 0
				&& desc.arrayLayers > 0
				&& std::has_single_bit(desc.samples);
		}
	}

	VkFormat ToVkFormat(Format format)
	{
		return Translate(FORMAT_TRANSLATIONS, format);
	}

	Format FromVkFormat(VkFormat format)
	{
		return TranslateBack(FORMAT_TRANSLATIONS, format);
	}

	ImageHandle VkRenderDevice::createImage(const ImageDesc& desc)
	{
		if (!IsValid(desc))
		{
			DEBUG_ERROR("Invalid image description ({}x{}, {} mips, {} layers)", desc.extent.width, desc.extent.height, desc.mipLevels, desc.arrayLayers);
			return {};
		}

		VkImageRecord record =
		{
			.format = ToVkFormat(desc.format),
			.extent = { desc.extent.width, desc.extent.height },
			.aspect = AspectOf(desc.format)
		};

		const bool                    sampled               = desc.usage & ImageUsageFlagBits::eSampled;
		const ImageUsageFlags         usage                 = sampled ? desc.usage | ImageUsageFlagBits::eTransferDst : desc.usage;
		const std::array<uint32_t, 2> upload_queue_families = { graphicsQueueFamilyIndex, transferQueueFamilyIndex };
		const bool                    shared_with_transfer  = (usage & ImageUsageFlagBits::eTransferDst)
			&& graphicsQueueFamilyIndex != transferQueueFamilyIndex;

		const VkImageCreateInfo image_info =
		{
			.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType             = VK_IMAGE_TYPE_2D,
			.format                = record.format,
			.extent                = { desc.extent.width, desc.extent.height, IMAGE_2D_DEPTH },
			.mipLevels             = desc.mipLevels,
			.arrayLayers           = desc.arrayLayers,
			.samples               = static_cast<VkSampleCountFlagBits>(desc.samples),
			.tiling                = VK_IMAGE_TILING_OPTIMAL,
			.usage                 = TranslateFlags(USAGE_TRANSLATIONS, usage),
			.sharingMode           = shared_with_transfer ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = shared_with_transfer ? static_cast<uint32_t>(upload_queue_families.size()) : 0,
			.pQueueFamilyIndices   = upload_queue_families.data(),
			.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
		};

		const VmaAllocationCreateInfo allocation_info =
		{
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VkResult result = vmaCreateImage(allocator, &image_info, &allocation_info, &record.image, &record.allocation, nullptr);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create {}x{} image: {}", desc.extent.width, desc.extent.height, string_VkResult(result));
			return {};
		}

		const VkImageViewCreateInfo view_info =
		{
			.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image            = record.image,
			.viewType         = desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
			.format           = record.format,
			.subresourceRange =
			{
				.aspectMask = record.aspect,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.layerCount = VK_REMAINING_ARRAY_LAYERS
			}
		};

		result = vkCreateImageView(device, &view_info, nullptr, &record.view);
		if (result != VK_SUCCESS)
		{
			DEBUG_ERROR("Failed to create image view: {}", string_VkResult(result));
			vmaDestroyImage(allocator, record.image, record.allocation);
			return {};
		}

		if (sampled && record.aspect == VK_IMAGE_ASPECT_COLOR_BIT)
			record.textureIndex = registerTexture(record.view);

		return registerImage(record);
	}

	void VkRenderDevice::destroyImage(ImageHandle image)
	{
		const PoolHandle<VkImageRecord> stored = FromGpuHandle(images, image);
		const VkImageRecord*            record = images.get(stored);
		if (record == nullptr)
			return;

		if (record->allocation == VK_NULL_HANDLE)
		{
			DEBUG_ERROR("Swapchain images are owned by their swapchain and can't be destroyed directly");
			return;
		}

		destroyLater(UploadTicket { record->lastUploadValue }, [this, view = record->view, vk_image = record->image, allocation = record->allocation, texture_index = record->textureIndex] {
			vkDestroyImageView(device, view, nullptr);
			vmaDestroyImage(allocator, vk_image, allocation);
			releaseTexture(texture_index);
		});

		images.destroy(stored);
	}

	Extent2D VkRenderDevice::getImageExtent(ImageHandle image)
	{
		const VkImageRecord* record = resolve(image);
		return record == nullptr ? Extent2D {} : Extent2D { record->extent.width, record->extent.height };
	}

	UploadTicket VkRenderDevice::uploadImage(ImageHandle image, const Rect2D& region, std::span<const std::byte> pixels)
	{
		VkImageRecord* record = resolve(image);
		DEBUG_ASSERT(record != nullptr, "Uploading to a null or destroyed image");

		const size_t expected_size = static_cast<size_t>(region.extent.width) * region.extent.height * BytesPerPixel(FromVkFormat(record->format));
		if (!Contains(record->extent, region) || pixels.size() != expected_size)
		{
			DEBUG_ERROR("Invalid image upload: {} bytes into a {}x{} region at ({}, {}) of a {}x{} image",
			            pixels.size(), region.extent.width, region.extent.height, region.offset.x, region.offset.y, record->extent.width, record->extent.height);
			return UploadTicket {};
		}

		const std::optional<VkBufferAllocation> staging = createStagingBuffer(pixels);
		if (!staging.has_value())
			return UploadTicket {};

		VkUploadBatch& batch = beginUploadBatch();
		TransitionImage(batch.commandBuffer, *record, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkBufferImageCopy copy =
		{
			.imageSubresource = { .aspectMask = record->aspect, .layerCount = 1 },
			.imageOffset      = { region.offset.x, region.offset.y, 0 },
			.imageExtent      = { region.extent.width, region.extent.height, IMAGE_2D_DEPTH }
		};

		vkCmdCopyBufferToImage(batch.commandBuffer, staging->buffer, record->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		TransitionImage(batch.commandBuffer, *record, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		batch.stagingBuffers.push_back(*staging);

		record->lastUploadValue = nextUploadValue;
		return UploadTicket { nextUploadValue };
	}

	VkImageRecord* VkRenderDevice::resolve(ImageHandle image)
	{
		return images.get(FromGpuHandle(images, image));
	}

	ImageHandle VkRenderDevice::registerImage(const VkImageRecord& record)
	{
		return ToGpuHandle<ImageTag>(images.create(record));
	}

	void VkRenderDevice::unregisterImage(ImageHandle image)
	{
		images.destroy(FromGpuHandle(images, image));
	}
}
