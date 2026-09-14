#include <lunar/render/render_device.hpp>
#include <lunar/debug.hpp>

#include "vk/vk_render_device.hpp"

namespace lunar::Render
{
	std::unique_ptr<RenderDevice> CreateRenderDevice(const RenderDeviceSettings& settings)
	{
		switch (settings.backend)
		{
		case Backend::eVulkan:
			return std::make_unique<imp::VkRenderDevice>(settings);
		default:
			DEBUG_ASSERT(false, "The requested render backend is not supported");
			return nullptr;
		}
	}
}
