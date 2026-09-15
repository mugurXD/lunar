#include "vk_render_device.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <lunar/debug.hpp>

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

		bool IsValid(const ImageDesc& desc)
		{
			return desc.extent.width > 0
				&& desc.extent.height > 0
				&& desc.format != Format::eUndefined
				&& desc.mipLevels > 0
				&& desc.arrayLayers > 0;
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

		const VkImageCreateInfo image_info =
		{
			.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType     = VK_IMAGE_TYPE_2D,
			.format        = record.format,
			.extent        = { desc.extent.width, desc.extent.height, IMAGE_2D_DEPTH },
			.mipLevels     = desc.mipLevels,
			.arrayLayers   = desc.arrayLayers,
			.samples       = VK_SAMPLE_COUNT_1_BIT,
			.tiling        = VK_IMAGE_TILING_OPTIMAL,
			.usage         = TranslateFlags(USAGE_TRANSLATIONS, desc.usage),
			.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
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

		destroyLater(UploadTicket {}, [this, view = record->view, vk_image = record->image, allocation = record->allocation] {
			vkDestroyImageView(device, view, nullptr);
			vmaDestroyImage(allocator, vk_image, allocation);
		});

		images.destroy(stored);
	}

	Extent2D VkRenderDevice::getImageExtent(ImageHandle image)
	{
		const VkImageRecord* record = resolve(image);
		return record == nullptr ? Extent2D {} : Extent2D { record->extent.width, record->extent.height };
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
