#pragma once
#include <lunar/api.hpp>
#include <lunar/render/render_device.hpp>

namespace lunar::Render
{
	class LUNAR_API Renderer
	{
	public:
		Renderer(RenderDevice& device, Swapchain* swapchain) noexcept;
		~Renderer() noexcept;

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		void render();

	private:
		void resizeDepthImage(Extent2D extent);
		void recordFrame(CommandList& commands, ImageHandle target) const;

		RenderDevice&  device;
		Swapchain*     swapchain        = nullptr;
		PipelineHandle trianglePipeline = {};
		ImageHandle    depthImage       = {};
	};
}
