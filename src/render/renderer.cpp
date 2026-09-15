#include <lunar/render/renderer.hpp>
#include <lunar/render/components.hpp>
#include <lunar/core/scene.hpp>
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
		constexpr Format           DEPTH_FORMAT            = Format::eD32Float;
		constexpr float            REVERSE_Z_CLEAR_DEPTH   = 0.f;
		constexpr CompareOp        REVERSE_Z_DEPTH_COMPARE = CompareOp::eGreater;

		struct MeshConstants
		{
			glm::mat4 modelViewProjection = glm::mat4(1.f);
			uint64_t  vertexAddress       = 0;
		};

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
		swapchain(swapchain),
		meshes(device)
	{
		cubeMesh = meshes.create(CreateCubeMeshData());

		if (swapchain == nullptr)
			return;

		const std::vector<char> vertex_shader   = LoadShader("mesh.vert");
		const std::vector<char> fragment_shader = LoadShader("mesh.frag");
		const Format            color_format    = swapchain->getFormat();

		meshPipeline = device.createGraphicsPipeline({
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&color_format, 1),
			.depthFormat    = DEPTH_FORMAT,
			.cullMode       = CullMode::eBack,
			.frontFace      = FrontFace::eCounterClockwise,
			.depthTest      = true,
			.depthWrite     = true,
			.depthCompare   = REVERSE_Z_DEPTH_COMPARE
		});
	}

	Renderer::~Renderer() noexcept
	{
		device.destroyImage(depthImage);
		device.destroyPipeline(meshPipeline);
	}

	void Renderer::render(Scene& scene)
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
		{
			const Extent2D extent = device.getImageExtent(backbuffer);
			resizeDepthImage(extent);
			recordFrame(frame.commandList(), scene, backbuffer, extent);
		}

		device.endFrame(frame);
	}

	MeshRegistry& Renderer::getMeshes()
	{
		return meshes;
	}

	MeshHandle Renderer::getCubeMesh() const
	{
		return cubeMesh;
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

	void Renderer::recordFrame(CommandList& commands, Scene& scene, ImageHandle target, Extent2D extent)
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

		const Camera* camera = scene.getMainCamera();
		if (camera != nullptr && meshPipeline != PipelineHandle {})
		{
			const glm::mat4 projection = camera->getProjectionMatrix(static_cast<int>(extent.width), static_cast<int>(extent.height));
			drawMeshes(commands, scene, projection * camera->getViewMatrix());
		}

		commands.endRendering();
	}

	void Renderer::drawMeshes(CommandList& commands, Scene& scene, const glm::mat4& view_projection)
	{
		commands.bindPipeline(meshPipeline);

		scene.forEach<MeshRenderer>([&](Entity entity, const MeshRenderer& mesh_renderer) {
			const Mesh* mesh = meshes.get(mesh_renderer.mesh);
			if (mesh == nullptr)
				return;

			commands.pushConstants(MeshConstants {
				.modelViewProjection = view_projection * scene.resolveWorldTransform(entity).matrix,
				.vertexAddress       = mesh->vertexAddress
			});
			commands.bindIndexBuffer(mesh->indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexed(mesh->indexCount);
		});
	}
}
