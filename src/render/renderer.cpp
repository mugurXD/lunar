#include <lunar/render/renderer.hpp>
#include <lunar/render/imgui_layer.hpp>
#include <lunar/render/components.hpp>
#include <lunar/render/shader.hpp>
#include <lunar/ui/ui_layer.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug.hpp>

#include <algorithm>
#include <bit>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

namespace lunar::Render
{
	namespace
	{
		const glm::vec4 CLEAR_COLOR = { 0.6f, 0.745f, 0.76f, 1.f };

		constexpr Format    DEPTH_FORMAT            = Format::eD32Float;
		constexpr Format    HDR_FORMAT              = Format::eRGBA16Float;
		constexpr float     REVERSE_Z_CLEAR_DEPTH   = 0.f;
		constexpr CompareOp REVERSE_Z_DEPTH_COMPARE = CompareOp::eGreater;
		constexpr uint32_t  SINGLE_SAMPLE           = 1;

		constexpr std::string_view DEFAULT_VERTEX_SHADER    = "mesh.vert";
		constexpr std::string_view DEFAULT_FRAGMENT_SHADER  = "mesh.frag";
		constexpr std::string_view LINE_FRAGMENT_SHADER     = "line.frag";
		constexpr std::string_view INSTANCED_VERTEX_SHADER  = "instanced.vert";
		constexpr std::string_view FULLSCREEN_VERTEX_SHADER = "fullscreen.vert";
		constexpr std::string_view TONEMAP_FRAGMENT_SHADER  = "tonemap.frag";
		constexpr uint32_t         FULLSCREEN_VERTEX_COUNT  = 3;
		constexpr size_t           STD430_MATRIX_ALIGN      = 16;

		const glm::vec4 AMBIENT_COLOR = { 0.08f, 0.08f, 0.1f, 0.f };

		struct SceneData
		{
			glm::mat4 viewProjection = glm::mat4(1.f);
			glm::vec4 lightDirection = {};
			glm::vec4 lightColor     = {};
			glm::vec4 skyColor       = {};
			glm::vec4 groundColor    = {};
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

		struct InstanceConstants
		{
			uint64_t                               sceneAddress    = 0;
			uint64_t                               instanceAddress = 0;
			uint64_t                               vertexAddress   = 0;
			alignas(STD430_MATRIX_ALIGN) glm::mat4 transform       = glm::mat4(1.f);
		};

		struct TonemapConstants
		{
			uint32_t sceneTexture = 0;
			float    exposure     = 1.f;
		};

		struct MeshInstance
		{
			const Mesh* mesh = nullptr;
			DrawData    draw = {};
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
				.skyColor       = AMBIENT_COLOR,
				.groundColor    = AMBIENT_COLOR,
				.cameraPosition = glm::vec4(camera.position, 1.f)
			};

			if (const DirectionalLight* light = FindFirst<DirectionalLight>(scene))
			{
				scene_data.lightDirection = glm::vec4(glm::normalize(light->direction), 0.f);
				scene_data.lightColor     = glm::vec4(light->color * light->intensity, 0.f);
			}

			if (const SkyLight* sky = FindFirst<SkyLight>(scene))
			{
				scene_data.skyColor    = glm::vec4(sky->skyColor * sky->intensity, 0.f);
				scene_data.groundColor = glm::vec4(sky->groundColor * sky->intensity, 0.f);
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

		float DistanceTo(const Bounds& bounds, const glm::vec3& point)
		{
			return glm::distance(point, glm::clamp(point, bounds.min, bounds.max));
		}

		MeshHandle SelectLod(std::span<const InstanceLod> lods, float distance)
		{
			const auto lod = std::ranges::find_if(lods, [distance](const InstanceLod& candidate) { return candidate.mesh != MeshHandle {} && distance <= candidate.maxDistance; });
			return lod == lods.end() ? MeshHandle {} : lod->mesh;
		}

		void DrawInstanced(Frame& frame, uint64_t scene_address, const Mesh& mesh, std::span<const DrawData> draws)
		{
			const uint64_t draw_address = frame.writeTransient(draws);
			if (draw_address == 0)
				return;

			CommandList& commands = frame.commandList();
			commands.pushConstants(MeshConstants { scene_address, draw_address, mesh.vertexAddress });
			commands.bindIndexBuffer(mesh.indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexed(mesh.indexCount, static_cast<uint32_t>(draws.size()));
		}
	}

	Renderer::Renderer(RenderDevice& device, Swapchain* swapchain, uint32_t samples) noexcept
		: device(device),
		swapchain(swapchain),
		meshes(device),
		lineMaterial { .vertexShader   = std::string(DEFAULT_VERTEX_SHADER),
		               .fragmentShader = std::string(LINE_FRAGMENT_SHADER),
		               .topology       = PrimitiveTopology::eLineList },
		instancedMaterial { .vertexShader   = std::string(INSTANCED_VERTEX_SHADER),
		                    .fragmentShader = std::string(DEFAULT_FRAGMENT_SHADER) }
	{
		cubeMesh = meshes.create(CreateCubeMeshData());
		setSamples(samples);
		defaultMaterial = createMaterial({ .vertexShader = DEFAULT_VERTEX_SHADER, .fragmentShader = DEFAULT_FRAGMENT_SHADER });
		createTonemapPipeline();
	}

	Renderer::~Renderer() noexcept
	{
		destroyTargets();
		destroyPipelines(lineMaterial);
		destroyPipelines(instancedMaterial);
		materials.forEach([this](PoolHandle<Material>, Material& material) {
			destroyPipelines(material);
		});
		device.destroyPipeline(tonemapPipeline);
	}

	void Renderer::setSamples(uint32_t samples)
	{
		const uint32_t supported = std::bit_floor(std::clamp(samples, SINGLE_SAMPLE, device.getCapabilities().maxSamples));
		if (supported == sampleCount)
			return;

		sampleCount = supported;
		destroyTargets();
		recreatePipelines(lineMaterial);
		recreatePipelines(instancedMaterial);
		materials.forEach([this](PoolHandle<Material>, Material& material) {
			recreatePipelines(material);
		});
	}

	uint32_t Renderer::getSamples() const
	{
		return sampleCount;
	}

	MaterialHandle Renderer::createMaterial(const MaterialDesc& desc)
	{
		const PoolHandle<Material> material = materials.create(Material { .vertexShader = std::string(desc.vertexShader), .fragmentShader = std::string(desc.fragmentShader) });
		createPipelines(*materials.get(material));
		return ToGpuHandle<MaterialTag>(material);
	}

	void Renderer::recreatePipelines(Material& material)
	{
		destroyPipelines(material);
		createPipelines(material);
	}

	void Renderer::createPipelines(Material& material)
	{
		if (swapchain == nullptr)
			return;

		const std::vector<char> vertex_shader   = LoadShader(material.vertexShader);
		const std::vector<char> fragment_shader = LoadShader(material.fragmentShader);

		const GraphicsPipelineDesc mesh_desc =
		{
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&HDR_FORMAT, 1),
			.depthFormat    = DEPTH_FORMAT,
			.topology       = material.topology,
			.cullMode       = CullMode::eBack,
			.frontFace      = FrontFace::eCounterClockwise,
			.depthTest      = true,
			.depthWrite     = true,
			.depthCompare   = REVERSE_Z_DEPTH_COMPARE,
			.samples        = sampleCount
		};

		GraphicsPipelineDesc translucent_desc = mesh_desc;
		translucent_desc.cullMode   = CullMode::eNone;
		translucent_desc.depthWrite = false;
		translucent_desc.blendMode  = BlendMode::eAlpha;

		material.opaquePipeline      = device.createGraphicsPipeline(mesh_desc);
		material.translucentPipeline = device.createGraphicsPipeline(translucent_desc);
	}

	void Renderer::destroyPipelines(Material& material)
	{
		device.destroyPipeline(material.translucentPipeline);
		device.destroyPipeline(material.opaquePipeline);
		material.translucentPipeline = {};
		material.opaquePipeline      = {};
	}

	void Renderer::createTonemapPipeline()
	{
		if (swapchain == nullptr)
			return;

		const std::vector<char> vertex_shader   = LoadShader(FULLSCREEN_VERTEX_SHADER);
		const std::vector<char> fragment_shader = LoadShader(TONEMAP_FRAGMENT_SHADER);
		const Format            target_format   = swapchain->getFormat();

		tonemapPipeline = device.createGraphicsPipeline({
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&target_format, 1)
		});
	}

	void Renderer::render(Scene& scene, UI::UiLayer* ui, ImGuiLayer* debug_ui)
	{
		Frame&            frame      = device.beginFrame();
		const ImageHandle backbuffer = swapchain != nullptr ? frame.acquire(*swapchain) : ImageHandle {};

		if (backbuffer != ImageHandle {})
		{
			const Extent2D extent = device.getImageExtent(backbuffer);
			resizeTargets(extent);
			recordFrame(frame, scene, extent);
			recordComposite(frame, scene, backbuffer, extent, ui, debug_ui);
		}

		device.endFrame(frame);
		lineVertices.clear();
	}

	MeshRegistry& Renderer::getMeshes()
	{
		return meshes;
	}

	MeshHandle Renderer::getCubeMesh() const
	{
		return cubeMesh;
	}

	void Renderer::drawLine(std::span<const glm::vec3> points, const glm::vec4& color)
	{
		for (size_t i = 1; i < points.size(); ++i)
			for (const glm::vec3& point : { points[i - 1], points[i] })
				lineVertices.push_back(Vertex { .position = point, .color = color });
	}

	void Renderer::resizeTargets(Extent2D extent)
	{
		if (device.getImageExtent(depthImage) == extent)
			return;

		destroyTargets();
		depthImage = device.createImage({
			.extent  = extent,
			.format  = DEPTH_FORMAT,
			.usage   = ImageUsageFlags(ImageUsageFlagBits::eDepthAttachment),
			.samples = sampleCount
		});

		hdrImage = device.createImage({
			.extent = extent,
			.format = HDR_FORMAT,
			.usage  = ImageUsageFlags(ImageUsageFlagBits::eColorAttachment) | ImageUsageFlagBits::eSampled
		});

		if (sampleCount > SINGLE_SAMPLE)
			colorImage = device.createImage({
				.extent  = extent,
				.format  = HDR_FORMAT,
				.usage   = ImageUsageFlags(ImageUsageFlagBits::eColorAttachment),
				.samples = sampleCount
			});
	}

	void Renderer::destroyTargets()
	{
		device.destroyImage(colorImage);
		device.destroyImage(hdrImage);
		device.destroyImage(depthImage);
		colorImage = {};
		hdrImage   = {};
		depthImage = {};
	}

	void Renderer::recordFrame(Frame& frame, Scene& scene, Extent2D extent)
	{
		CommandList&       commands = frame.commandList();
		const DistanceFog* fog      = FindFirst<DistanceFog>(scene);

		const bool            multisampled     = colorImage != ImageHandle {};
		const ColorAttachment color_attachment =
		{
			.image        = multisampled ? colorImage : hdrImage,
			.loadOp       = LoadOp::eClear,
			.clearColor   = fog != nullptr ? glm::vec4(fog->color, 1.f) : CLEAR_COLOR,
			.resolveImage = multisampled ? hdrImage : ImageHandle {}
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
		if (camera != nullptr)
		{
			const SceneData scene_data    = BuildSceneData(scene, *camera, extent);
			const uint64_t  scene_address = frame.writeTransient(scene_data);
			if (scene_address != 0)
			{
				const Frustum frustum(scene_data.viewProjection);
				const auto    draw_materials = [&](bool translucent) {
					materials.forEach([&](PoolHandle<Material> handle, const Material& material) {
						const PipelineHandle pipeline = translucent ? material.translucentPipeline : material.opaquePipeline;
						drawMeshes(frame, scene, scene_address, frustum, ToGpuHandle<MaterialTag>(handle), pipeline, translucent);
					});
				};

				draw_materials(false);
				drawInstances(frame, scene, scene_address, frustum, camera->position);
				draw_materials(true);
				drawLines(frame, scene_address);
			}
		}

		commands.endRendering();
	}

	void Renderer::recordComposite(Frame& frame, Scene& scene, ImageHandle target, Extent2D extent, UI::UiLayer* ui, ImGuiLayer* debug_ui)
	{
		CommandList&          commands          = frame.commandList();
		const Camera*         camera            = scene.getMainCamera();
		const ColorAttachment target_attachment = { .image = target, .loadOp = LoadOp::eClear };

		commands.prepareSampling(hdrImage);
		commands.beginRendering({ .colorAttachments = std::span(&target_attachment, 1) });

		if (tonemapPipeline != PipelineHandle {})
		{
			commands.bindPipeline(tonemapPipeline);
			commands.pushConstants(TonemapConstants { device.getTextureIndex(hdrImage), camera != nullptr ? camera->exposure : Camera {}.exposure });
			commands.draw(FULLSCREEN_VERTEX_COUNT);
		}

		if (ui != nullptr)
			ui->record(commands, extent);

		if (debug_ui != nullptr)
			debug_ui->record(commands);

		commands.endRendering();
	}

	void Renderer::drawMeshes(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum, MaterialHandle material, PipelineHandle pipeline, bool translucent)
	{
		if (pipeline == PipelineHandle {})
			return;

		std::vector<MeshInstance> instances;
		scene.forEach<MeshRenderer>([&](Entity entity, const MeshRenderer& mesh_renderer) {
			const Mesh*          mesh  = meshes.get(mesh_renderer.mesh);
			const MaterialHandle drawn = mesh_renderer.material == MaterialHandle {} ? defaultMaterial : mesh_renderer.material;
			if (mesh == nullptr || !mesh_renderer.visible || mesh_renderer.translucent != translucent || drawn != material)
				return;

			const glm::mat4 model = scene.resolveWorldTransform(entity).matrix;
			if (frustum.intersects(mesh->bounds.transformed(model)))
				instances.push_back({ mesh, { model, NormalMatrix(model) } });
		});

		if (instances.empty())
			return;

		// Stable so translucent instances of the same mesh keep a consistent blend order between frames.
		std::ranges::stable_sort(instances, {}, &MeshInstance::mesh);
		frame.commandList().bindPipeline(pipeline);

		std::vector<DrawData> draws;
		for (auto first = instances.begin(); first != instances.end();)
		{
			const Mesh* mesh = first->mesh;
			const auto  last = std::ranges::find_if(first, instances.end(), [mesh](const Mesh* other) { return other != mesh; }, &MeshInstance::mesh);

			draws.clear();
			std::ranges::transform(first, last, std::back_inserter(draws), &MeshInstance::draw);
			DrawInstanced(frame, scene_address, *mesh, draws);
			first = last;
		}
	}

	void Renderer::drawInstances(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum, const glm::vec3& eye)
	{
		if (instancedMaterial.opaquePipeline == PipelineHandle {})
			return;

		CommandList& commands = frame.commandList();
		commands.bindPipeline(instancedMaterial.opaquePipeline);

		scene.forEach<InstanceBatch>([&](Entity entity, const InstanceBatch& batch) {
			const InstanceSet* instances = meshes.get(batch.instances);
			if (!batch.visible || instances == nullptr)
				return;

			const glm::mat4 transform = scene.resolveWorldTransform(entity).matrix;
			const Bounds    bounds    = batch.bounds.transformed(transform);
			const Mesh*     mesh      = meshes.get(SelectLod(batch.lods, DistanceTo(bounds, eye)));
			if (mesh == nullptr || !frustum.intersects(bounds))
				return;

			commands.pushConstants(InstanceConstants { scene_address, instances->address, mesh->vertexAddress, transform });
			commands.bindIndexBuffer(mesh->indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexed(mesh->indexCount, instances->count);
		});
	}

	void Renderer::drawLines(Frame& frame, uint64_t scene_address)
	{
		if (lineVertices.empty() || lineMaterial.opaquePipeline == PipelineHandle {})
			return;

		const uint64_t vertex_address = frame.writeTransient(std::span<const Vertex>(lineVertices));
		const uint64_t draw_address   = frame.writeTransient(DrawData {});
		if (vertex_address == 0 || draw_address == 0)
			return;

		CommandList& commands = frame.commandList();
		commands.bindPipeline(lineMaterial.opaquePipeline);
		commands.pushConstants(MeshConstants { scene_address, draw_address, vertex_address });
		commands.draw(static_cast<uint32_t>(lineVertices.size()));
	}
}
