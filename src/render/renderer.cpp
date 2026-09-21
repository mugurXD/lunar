#include <lunar/render/renderer.hpp>
#include <lunar/render/imgui_layer.hpp>
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
		const glm::vec4 CLEAR_COLOR = { 0.6f, 0.745f, 0.76f, 1.f };

		constexpr std::string_view SHADER_BINARY_PATH      = "shader-bin/{}.spv";
		constexpr Format           DEPTH_FORMAT            = Format::eD32Float;
		constexpr float            REVERSE_Z_CLEAR_DEPTH   = 0.f;
		constexpr CompareOp        REVERSE_Z_DEPTH_COMPARE = CompareOp::eGreater;

		const glm::vec4 AMBIENT_COLOR = { 0.08f, 0.08f, 0.1f, 0.f };

		struct SceneData
		{
			glm::mat4 viewProjection = glm::mat4(1.f);
			glm::vec4 lightDirection = {};
			glm::vec4 lightColor     = {};
			glm::vec4 ambientColor   = {};
			glm::vec4 cameraPosition = {};
			glm::vec4 fogColor       = {};
			glm::vec4 fogRange       = {};
		};

		struct DrawData
		{
			glm::mat4 model        = glm::mat4(1.f);
			glm::mat4 normalMatrix = glm::mat4(1.f);
		};

		struct MeshConstants
		{
			uint64_t sceneAddress  = 0;
			uint64_t drawAddress   = 0;
			uint64_t vertexAddress = 0;
		};

		template<typename T>
		const T* FindFirst(Scene& scene)
		{
			const T* found = nullptr;
			scene.forEach<T>([&found](Entity, const T& candidate) {
				if (found == nullptr)
					found = &candidate;
			});

			return found;
		}

		SceneData BuildSceneData(Scene& scene, const Camera& camera, Extent2D extent)
		{
			const glm::mat4 projection = camera.getProjectionMatrix(static_cast<int>(extent.width), static_cast<int>(extent.height));

			SceneData scene_data =
			{
				.viewProjection = projection * camera.getViewMatrix(),
				.ambientColor   = AMBIENT_COLOR,
				.cameraPosition = glm::vec4(camera.position, 1.f)
			};

			if (const DirectionalLight* light = FindFirst<DirectionalLight>(scene))
			{
				scene_data.lightDirection = glm::vec4(glm::normalize(light->direction), 0.f);
				scene_data.lightColor     = glm::vec4(light->color * light->intensity, 0.f);
			}

			if (const DistanceFog* fog = FindFirst<DistanceFog>(scene))
			{
				scene_data.fogColor = glm::vec4(fog->color, 1.f);
				scene_data.fogRange = glm::vec4(fog->startDistance, fog->endDistance, 0.f, 0.f);
			}

			return scene_data;
		}

		glm::mat4 NormalMatrix(const glm::mat4& model)
		{
			if (glm::mat3(model) == glm::mat3(1.f))
				return glm::mat4(1.f);

			return glm::transpose(glm::inverse(model));
		}

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

	void Renderer::render(Scene& scene, ImGuiLayer* ui)
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
		{
			const Extent2D extent = device.getImageExtent(backbuffer);
			resizeDepthImage(extent);
			recordFrame(frame, scene, backbuffer, extent);

			if (ui != nullptr)
				recordOverlay(frame, backbuffer, *ui);
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

	void Renderer::recordFrame(Frame& frame, Scene& scene, ImageHandle target, Extent2D extent)
	{
		CommandList&       commands = frame.commandList();
		const DistanceFog* fog      = FindFirst<DistanceFog>(scene);

		const ColorAttachment color_attachment =
		{
			.image      = target,
			.loadOp     = LoadOp::eClear,
			.clearColor = fog != nullptr ? glm::vec4(fog->color, 1.f) : CLEAR_COLOR
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
			const SceneData scene_data    = BuildSceneData(scene, *camera, extent);
			const uint64_t  scene_address = frame.writeTransient(scene_data);
			if (scene_address != 0)
				drawMeshes(frame, scene, scene_address, Frustum(scene_data.viewProjection));
		}

		commands.endRendering();
	}

	void Renderer::recordOverlay(Frame& frame, ImageHandle target, ImGuiLayer& ui)
	{
		CommandList&          commands           = frame.commandList();
		const ColorAttachment overlay_attachment = { .image = target, .loadOp = LoadOp::eLoad };

		commands.beginRendering({ .colorAttachments = std::span(&overlay_attachment, 1) });
		ui.record(commands);
		commands.endRendering();
	}

	void Renderer::drawMeshes(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum)
	{
		CommandList& commands = frame.commandList();
		commands.bindPipeline(meshPipeline);

		scene.forEach<MeshRenderer>([&](Entity entity, const MeshRenderer& mesh_renderer) {
			const Mesh* mesh = meshes.get(mesh_renderer.mesh);
			if (mesh == nullptr || !mesh_renderer.visible)
				return;

			const glm::mat4 model = scene.resolveWorldTransform(entity).matrix;
			if (!frustum.intersects(mesh->bounds.transformed(model)))
				return;

			const uint64_t draw_address = frame.writeTransient(DrawData { model, NormalMatrix(model) });
			if (draw_address == 0)
				return;

			commands.pushConstants(MeshConstants { scene_address, draw_address, mesh->vertexAddress });
			commands.bindIndexBuffer(mesh->indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexed(mesh->indexCount);
		});
	}
}
