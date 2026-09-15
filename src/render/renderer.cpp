#include <lunar/render/renderer.hpp>
#include <lunar/file/binary_file.hpp>
#include <lunar/debug.hpp>

#include <format>
#include <string_view>
#include <vector>

namespace lunar::Render
{
	namespace
	{
		const glm::vec4 CLEAR_COLOR = { 1.f, 0.f, 0.f, 1.f };

		constexpr std::string_view SHADER_BINARY_PATH    = "shader-bin/{}.spv";
		constexpr uint32_t         TRIANGLE_VERTEX_COUNT = 3;

		std::vector<char> LoadShader(std::string_view name)
		{
			const Fs::Path path = Fs::fromData(std::format(SHADER_BINARY_PATH, name));

			Fs::BinaryFile file(path);
			if (file.content.empty())
				DEBUG_ERROR("Failed to load shader '{}'", path.string());

			return std::move(file.content);
		}
	}

	Renderer::Renderer(RenderDevice& device, Swapchain* swapchain) noexcept
		: device(device),
		swapchain(swapchain)
	{
		if (swapchain == nullptr)
			return;

		const std::vector<char> vertex_shader   = LoadShader("triangle.vert");
		const std::vector<char> fragment_shader = LoadShader("triangle.frag");
		const Format            color_format    = swapchain->getFormat();

		trianglePipeline = device.createGraphicsPipeline({
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&color_format, 1)
		});
	}

	Renderer::~Renderer() noexcept
	{
		device.destroyPipeline(trianglePipeline);
	}

	void Renderer::render()
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
			recordFrame(frame.commandList(), backbuffer);

		device.endFrame(frame);
	}

	void Renderer::recordFrame(CommandList& commands, ImageHandle target) const
	{
		const ColorAttachment color_attachment =
		{
			.image      = target,
			.loadOp     = LoadOp::eClear,
			.clearColor = CLEAR_COLOR
		};

		commands.beginRendering({ .colorAttachments = std::span(&color_attachment, 1) });

		if (trianglePipeline != PipelineHandle {})
		{
			commands.bindPipeline(trianglePipeline);
			commands.draw(TRIANGLE_VERTEX_COUNT);
		}

		commands.endRendering();
	}
}
