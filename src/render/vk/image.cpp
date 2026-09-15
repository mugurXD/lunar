#include "vk_render_device.hpp"

namespace lunar::Render::imp
{
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
