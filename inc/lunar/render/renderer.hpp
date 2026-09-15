#pragma once
#include <lunar/api.hpp>
#include <lunar/render/render_device.hpp>

namespace lunar::Render
{
	class LUNAR_API Renderer
	{
	public:
		Renderer(RenderDevice& device) noexcept;

		void render(Swapchain* swapchain);

	private:
		RenderDevice& device;
	};
}
