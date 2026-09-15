#include <lunar/render/renderer.hpp>

namespace lunar::Render
{
	namespace
	{
		const glm::vec4 CLEAR_COLOR = { 1.f, 0.f, 0.f, 1.f };
	}

	Renderer::Renderer(RenderDevice& device) noexcept
		: device(device)
	{
	}

	void Renderer::render(Swapchain* swapchain)
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
		{
			const ColorAttachment target =
			{
				.image      = backbuffer,
				.loadOp     = LoadOp::eClear,
				.clearColor = CLEAR_COLOR
			};

			CommandList& commands = frame.commandList();
			commands.beginRendering({ .colorAttachments = std::span(&target, 1) });
			commands.endRendering();
		}

		device.endFrame(frame);
	}
}
