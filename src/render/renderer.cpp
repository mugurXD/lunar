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

		constexpr std::string_view SHADER_BINARY_PATH      = "shader-bin/{}.spv";
		constexpr uint32_t         TRIANGLE_VERTEX_COUNT   = 3;
		constexpr uint32_t         TRIANGLE_INSTANCE_COUNT = 2;
		constexpr Format           DEPTH_FORMAT            = Format::eD32Float;
		constexpr float            REVERSE_Z_CLEAR_DEPTH   = 0.f;
		constexpr CompareOp        REVERSE_Z_DEPTH_COMPARE = CompareOp::eGreater;

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
			.colorFormats   = std::span(&color_format, 1),
			.depthFormat    = DEPTH_FORMAT,
			.depthTest      = true,
			.depthWrite     = true,
			.depthCompare   = REVERSE_Z_DEPTH_COMPARE
		});
	}

	Renderer::~Renderer() noexcept
	{
		device.destroyImage(depthImage);
		device.destroyPipeline(trianglePipeline);
	}

	void Renderer::render()
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
		{
			resizeDepthImage(device.getImageExtent(backbuffer));
			recordFrame(frame.commandList(), backbuffer);
		}

		device.endFrame(frame);
	}

	void Renderer::resizeDepthImage(Extent2D extent)
	{
		if (device.getImageExtent(depthImage) == extent)
			return;

		device.destroyImage(depthImage);
		depthImage = device.createImage({
			.extent = extent,
			.format = DEPTH_FORMAT,
			.usage  = ImageUsageFlags(ImageUsageFlagBits::eDepthAttachment)
		});
	}

	void Renderer::recordFrame(CommandList& commands, ImageHandle target) const
	{
		const ColorAttachment color_attachment =
		{
			.image      = target,
			.loadOp     = LoadOp::eClear,
			.clearColor = CLEAR_COLOR
		};

		const DepthAttachment depth_attachment =
		{
			.image      = depthImage,
			.loadOp     = LoadOp::eClear,
			.clearDepth = REVERSE_Z_CLEAR_DEPTH
		};

		commands.beginRendering({
			.colorAttachments = std::span(&color_attachment, 1),
			.depthAttachment  = depth_attachment
		});

		if (trianglePipeline != PipelineHandle {})
		{
			commands.bindPipeline(trianglePipeline);
			commands.draw(TRIANGLE_VERTEX_COUNT, TRIANGLE_INSTANCE_COUNT);
		}

		commands.endRendering();
	}
}
