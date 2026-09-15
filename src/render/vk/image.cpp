#include "vk_render_device.hpp"

namespace lunar::Render::imp
{
	namespace
	{
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
	}

	VkFormat ToVkFormat(Format format)
	{
		return Translate(FORMAT_TRANSLATIONS, format);
	}

	Format FromVkFormat(VkFormat format)
	{
		return TranslateBack(FORMAT_TRANSLATIONS, format);
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
