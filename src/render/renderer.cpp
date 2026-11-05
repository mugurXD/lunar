#include <lunar/render/renderer.hpp>
#include <lunar/render/lod_fade.hpp>
#include <lunar/render/imgui_layer.hpp>
#include <lunar/render/components.hpp>
#include <lunar/render/shader.hpp>
#include <lunar/ui/ui_layer.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
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
		constexpr Format    DEPTH_TEXTURE_FORMAT    = Format::eR32Float;
		constexpr Format    OCCLUSION_FORMAT        = Format::eR32Float;
		constexpr uint32_t  SHADOW_MAP_RESOLUTION   = 2048;
		constexpr Extent2D  SHADOW_MAP_EXTENT       = { SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION };
		constexpr float     SHADOW_FADE_START       = 0.9f;
		constexpr float     OCCLUSION_FADE_START    = 0.8f;
		constexpr uint32_t  MIN_TARGET_SIZE         = 1;
		constexpr float     REVERSE_Z_CLEAR_DEPTH   = 0.f;
		constexpr CompareOp REVERSE_Z_DEPTH_COMPARE = CompareOp::eGreater;
		constexpr uint32_t  SINGLE_SAMPLE           = 1;

		constexpr std::string_view DEFAULT_VERTEX_SHADER    = "mesh.vert";
		constexpr std::string_view DEFAULT_FRAGMENT_SHADER  = "mesh.frag";
		constexpr std::string_view LINE_FRAGMENT_SHADER     = "line.frag";
		constexpr std::string_view INSTANCED_VERTEX_SHADER  = "instanced.vert";
		constexpr std::string_view FULLSCREEN_VERTEX_SHADER = "fullscreen.vert";
		constexpr std::string_view TONEMAP_FRAGMENT_SHADER  = "tonemap.frag";
		constexpr std::string_view SHADOW_FRAGMENT_SHADER   = "shadow.frag";
		constexpr std::string_view INSTANCED_FADE_SHADER    = "instanced_fade.frag";
		constexpr std::string_view SHADOW_FADE_SHADER       = "shadow_fade.frag";
		constexpr std::string_view OCCLUSION_SHADER         = "ambient_occlusion.frag";
		constexpr std::string_view OCCLUSION_BLUR_SHADER    = "ambient_occlusion_blur.frag";
		constexpr uint32_t         FULLSCREEN_VERTEX_COUNT  = 3;
		constexpr size_t           STD430_MATRIX_ALIGN      = 16;

		const glm::vec4 AMBIENT_COLOR = { 0.08f, 0.08f, 0.1f, 0.f };

		struct alignas(STD430_MATRIX_ALIGN) ShadowCascadeData
		{
			glm::mat4 viewProjection = glm::mat4(1.f);
			float     texelSize      = 0.f;
			uint32_t  shadowMap      = 0;
		};

		struct SceneData
		{
			glm::mat4                                           viewProjection = glm::mat4(1.f);
			glm::vec4                                           lightDirection = {};
			glm::vec4                                           lightColor     = {};
			glm::vec4                                           skyColor       = {};
			glm::vec4                                           groundColor    = {};
			glm::vec4                                           cameraPosition = {};
			glm::vec4                                           fogColor       = {};
			glm::vec4                                           fogRange       = {};
			std::array<ShadowCascadeData, SHADOW_CASCADE_COUNT> shadowCascades   = {};
			glm::vec4                                           shadowRange      = {};
			glm::vec2                                           screenTexelSize  = {};
			uint32_t                                            ambientOcclusion = INVALID_TEXTURE_INDEX;
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
			glm::vec4                              fadeBands       = {};
		};

		struct InstanceDraw
		{
			InstanceConstants constants     = {};
			const Mesh*       mesh          = nullptr;
			uint32_t          instanceCount = 0;
		};

		struct TonemapConstants
		{
			uint32_t sceneTexture = 0;
			float    exposure     = 1.f;
		};

		struct AmbientOcclusionConstants
		{
			glm::vec2 tanHalfFov       = {};
			glm::vec2 texelSize        = {};
			glm::vec2 fadeRange        = {};
			float     nearPlane        = 0.f;
			float     radius           = 0.f;
			float     intensity        = 0.f;
			uint32_t  depthTexture     = 0;
			uint32_t  occlusionTexture = 0;
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

		glm::mat4 ViewProjection(const Camera& camera, Extent2D extent)
		{
			return camera.getProjectionMatrix(static_cast<int>(extent.width), static_cast<int>(extent.height)) * camera.getViewMatrix();
		}

		// The camera projection is reverse Z with an infinite far plane, so culling at a distance needs a finite reverse Z projection, which swaps near and far.
		glm::mat4 BoundedViewProjection(const Camera& camera, float aspect_ratio, float far_distance)
		{
			return glm::perspectiveRH_ZO(glm::radians(camera.fov), aspect_ratio, far_distance, camera.nearPlane) * camera.getViewMatrix();
		}

		float AspectRatio(Extent2D extent)
		{
			return static_cast<float>(extent.width) / static_cast<float>(extent.height);
		}

		Extent2D ScaleExtent(Extent2D extent, float scale)
		{
			const auto scaled = [scale](uint32_t size) {
				return std::max(MIN_TARGET_SIZE, static_cast<uint32_t>(std::ceil(static_cast<float>(size) * scale)));
			};

			return { scaled(extent.width), scaled(extent.height) };
		}

		void DestroyImages(RenderDevice& device, std::initializer_list<ImageHandle*> images)
		{
			for (ImageHandle* image : images)
			{
				device.destroyImage(*image);
				*image = {};
			}
		}

		SceneData BuildSceneData(Scene& scene, const Camera& camera, Extent2D extent)
		{
			SceneData scene_data =
			{
				.viewProjection  = ViewProjection(camera, extent),
				.skyColor        = AMBIENT_COLOR,
				.groundColor     = AMBIENT_COLOR,
				.cameraPosition  = glm::vec4(camera.position, 1.f),
				.screenTexelSize = 1.f / glm::vec2(extent.width, extent.height)
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

		float FarthestDistanceTo(const Bounds& bounds, const glm::vec3& point)
		{
			return glm::length(glm::max(glm::abs(point - bounds.min), glm::abs(point - bounds.max)));
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

		PipelineHandle CreateFullscreenPipeline(RenderDevice& device, std::string_view fragment_shader_name, Format target_format)
		{
			const std::vector<char> vertex_shader   = LoadShader(FULLSCREEN_VERTEX_SHADER);
			const std::vector<char> fragment_shader = LoadShader(fragment_shader_name);

			return device.createGraphicsPipeline({
				.vertexShader   = std::as_bytes(std::span(vertex_shader)),
				.fragmentShader = std::as_bytes(std::span(fragment_shader)),
				.colorFormats   = std::span(&target_format, 1)
			});
		}

		template<typename T>
		void DrawFullscreen(CommandList& commands, PipelineHandle pipeline, ImageHandle target, const T& constants)
		{
			const ColorAttachment attachment = { .image = target, .loadOp = LoadOp::eClear };

			commands.beginRendering({ .colorAttachments = std::span(&attachment, 1) });
			commands.bindPipeline(pipeline);
			commands.pushConstants(constants);
			commands.draw(FULLSCREEN_VERTEX_COUNT);
			commands.endRendering();
			commands.prepareSampling(target);
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
		                    .fragmentShader = std::string(DEFAULT_FRAGMENT_SHADER) },
		fadingInstancedMaterial { .vertexShader         = std::string(INSTANCED_VERTEX_SHADER),
		                          .fragmentShader       = std::string(INSTANCED_FADE_SHADER),
		                          .shadowFragmentShader = std::string(SHADOW_FADE_SHADER) }
	{
		cubeMesh = meshes.create(CreateCubeMeshData());
		setSamples(samples);
		defaultMaterial = createMaterial({ .vertexShader = DEFAULT_VERTEX_SHADER, .fragmentShader = DEFAULT_FRAGMENT_SHADER });
		createFullscreenPipelines();
	}

	Renderer::~Renderer() noexcept
	{
		destroyTargets();
		destroyPipelines(lineMaterial);
		destroyPipelines(instancedMaterial);
		destroyPipelines(fadingInstancedMaterial);
		materials.forEach([this](PoolHandle<Material>, Material& material) {
			destroyPipelines(material);
		});
		device.destroyPipeline(tonemapPipeline);
		device.destroyPipeline(occlusionPipeline);
		device.destroyPipeline(occlusionBlurPipeline);

		for (const ImageHandle shadow_map : shadowMaps)
			device.destroyImage(shadow_map);

		device.destroyImage(shadowDepthImage);
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
		recreatePipelines(fadingInstancedMaterial);
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
		const std::vector<char> shadow_shader   = LoadShader(material.shadowFragmentShader.empty() ? SHADOW_FRAGMENT_SHADER : material.shadowFragmentShader);

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

		GraphicsPipelineDesc shadow_desc = mesh_desc;
		shadow_desc.fragmentShader = std::as_bytes(std::span(shadow_shader));
		shadow_desc.colorFormats   = std::span(&DEPTH_TEXTURE_FORMAT, 1);
		shadow_desc.samples        = SINGLE_SAMPLE;

		// The depth prepass has to cover exactly what the main pass shows, so it keeps back face culling.
		GraphicsPipelineDesc depth_desc = shadow_desc;

		// Single sided meshes still have to block the sun when it shines on their back.
		shadow_desc.cullMode = CullMode::eNone;

		material.opaquePipeline      = device.createGraphicsPipeline(mesh_desc);
		material.translucentPipeline = device.createGraphicsPipeline(translucent_desc);
		material.shadowPipeline      = device.createGraphicsPipeline(shadow_desc);
		material.depthPipeline       = device.createGraphicsPipeline(depth_desc);
	}

	void Renderer::destroyPipelines(Material& material)
	{
		device.destroyPipeline(material.depthPipeline);
		device.destroyPipeline(material.shadowPipeline);
		device.destroyPipeline(material.translucentPipeline);
		device.destroyPipeline(material.opaquePipeline);
		material.depthPipeline       = {};
		material.shadowPipeline      = {};
		material.translucentPipeline = {};
		material.opaquePipeline      = {};
	}

	void Renderer::createFullscreenPipelines()
	{
		if (swapchain == nullptr)
			return;

		tonemapPipeline       = CreateFullscreenPipeline(device, TONEMAP_FRAGMENT_SHADER, swapchain->getFormat());
		occlusionPipeline     = CreateFullscreenPipeline(device, OCCLUSION_SHADER, OCCLUSION_FORMAT);
		occlusionBlurPipeline = CreateFullscreenPipeline(device, OCCLUSION_BLUR_SHADER, OCCLUSION_FORMAT);
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
		DestroyImages(device, { &colorImage, &hdrImage, &depthImage, &prepassDepthImage, &viewDepthImage, &rawOcclusionImage, &occlusionImage });
	}

	void Renderer::recordFrame(Frame& frame, Scene& scene, Extent2D extent)
	{
		CommandList&         commands  = frame.commandList();
		const DistanceFog*   fog       = FindFirst<DistanceFog>(scene);
		const Camera*        camera    = scene.getMainCamera();
		const SceneAddresses addresses = camera != nullptr ? prepareSceneData(frame, scene, *camera, extent) : SceneAddresses {};

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

		if (addresses.opaque != 0 && addresses.translucent != 0)
		{
			const Frustum frustum(ViewProjection(*camera, extent));
			drawOpaque(frame, scene, addresses.opaque, frustum, camera->position, &Material::opaquePipeline);
			materials.forEach([&](PoolHandle<Material> handle, const Material& material) {
				drawMeshes(frame, scene, addresses.translucent, frustum, ToGpuHandle<MaterialTag>(handle), material.translucentPipeline, true);
			});
			drawLines(frame, addresses.opaque);
		}

		commands.endRendering();
	}

	Renderer::SceneAddresses Renderer::prepareSceneData(Frame& frame, Scene& scene, const Camera& camera, Extent2D extent)
	{
		SceneData               scene_data = BuildSceneData(scene, camera, extent);
		const DirectionalLight* light      = FindFirst<DirectionalLight>(scene);
		const CascadedShadows*  shadows    = FindFirst<CascadedShadows>(scene);
		if (light != nullptr && shadows != nullptr)
		{
			const auto cascades = FitShadowCascades(camera, AspectRatio(extent), light->direction, *shadows, SHADOW_MAP_RESOLUTION);
			recordShadows(frame, scene, camera.position, cascades);

			for (size_t index = 0; index < cascades.size(); index++)
				scene_data.shadowCascades[index] = { cascades[index].viewProjection, cascades[index].texelSize, device.getTextureIndex(shadowMaps[index]) };

			scene_data.shadowRange = glm::vec4(shadows->distance * SHADOW_FADE_START, shadows->distance, 0.f, 0.f);
		}

		// Translucent surfaces are missing from the depth the occlusion is traced against, so they would pick up the occlusion of whatever lies behind them.
		const uint64_t translucent_address = frame.writeTransient(scene_data);
		if (const AmbientOcclusion* occlusion = FindFirst<AmbientOcclusion>(scene))
			scene_data.ambientOcclusion = recordAmbientOcclusion(frame, scene, camera, *occlusion, extent, translucent_address);

		return { frame.writeTransient(scene_data), translucent_address };
	}

	void Renderer::recordShadows(Frame& frame, Scene& scene, const glm::vec3& eye, std::span<const ShadowCascade> cascades)
	{
		if (shadowDepthImage == ImageHandle {})
			createShadowTargets();

		CommandList& commands = frame.commandList();
		for (size_t index = 0; index < cascades.size(); index++)
		{
			const ColorAttachment shadow_attachment = { .image = shadowMaps[index], .loadOp = LoadOp::eClear, .clearColor = glm::vec4(REVERSE_Z_CLEAR_DEPTH) };
			const DepthAttachment depth_attachment  = { .image = shadowDepthImage, .loadOp = LoadOp::eClear, .clearDepth = REVERSE_Z_CLEAR_DEPTH };
			const uint64_t        scene_address     = frame.writeTransient(SceneData { .viewProjection = cascades[index].viewProjection, .cameraPosition = glm::vec4(eye, 1.f) });

			commands.beginRendering({ .colorAttachments = std::span(&shadow_attachment, 1), .depthAttachment = depth_attachment });

			if (scene_address != 0)
				drawOpaque(frame, scene, scene_address, Frustum(cascades[index].viewProjection), eye, &Material::shadowPipeline);

			commands.endRendering();
			commands.prepareSampling(shadowMaps[index]);
		}
	}

	uint32_t Renderer::recordAmbientOcclusion(Frame&                  frame,
	                                          Scene&                  scene,
	                                          const Camera&           camera,
	                                          const AmbientOcclusion& occlusion,
	                                          Extent2D                extent,
	                                          uint64_t                scene_address)
	{
		if (occlusionPipeline == PipelineHandle {} || occlusionBlurPipeline == PipelineHandle {} || scene_address == 0)
			return INVALID_TEXTURE_INDEX;

		const Extent2D occlusion_extent = ScaleExtent(extent, occlusion.resolutionScale);
		if (device.getImageExtent(viewDepthImage) != occlusion_extent)
			createOcclusionTargets(occlusion_extent);

		CommandList&          commands         = frame.commandList();
		const ColorAttachment view_depth       = { .image = viewDepthImage, .loadOp = LoadOp::eClear, .clearColor = glm::vec4(REVERSE_Z_CLEAR_DEPTH) };
		const DepthAttachment depth_attachment = { .image = prepassDepthImage, .loadOp = LoadOp::eClear, .clearDepth = REVERSE_Z_CLEAR_DEPTH };
		const Frustum         frustum(BoundedViewProjection(camera, AspectRatio(extent), occlusion.distance));

		commands.beginRendering({ .colorAttachments = std::span(&view_depth, 1), .depthAttachment = depth_attachment });
		drawOpaque(frame, scene, scene_address, frustum, camera.position, &Material::depthPipeline);
		commands.endRendering();
		commands.prepareSampling(viewDepthImage);

		const glm::mat4                 projection = camera.getProjectionMatrix(static_cast<int>(extent.width), static_cast<int>(extent.height));
		const AmbientOcclusionConstants constants  =
		{
			.tanHalfFov       = glm::vec2(1.f / projection[0][0], 1.f / projection[1][1]),
			.texelSize        = 1.f / glm::vec2(occlusion_extent.width, occlusion_extent.height),
			.fadeRange        = glm::vec2(occlusion.distance * OCCLUSION_FADE_START, occlusion.distance),
			.nearPlane        = camera.nearPlane,
			.radius           = occlusion.radius,
			.intensity        = occlusion.intensity,
			.depthTexture     = device.getTextureIndex(viewDepthImage),
			.occlusionTexture = device.getTextureIndex(rawOcclusionImage)
		};

		DrawFullscreen(commands, occlusionPipeline, rawOcclusionImage, constants);
		DrawFullscreen(commands, occlusionBlurPipeline, occlusionImage, constants);
		return device.getTextureIndex(occlusionImage);
	}

	void Renderer::createOcclusionTargets(Extent2D extent)
	{
		DestroyImages(device, { &prepassDepthImage, &viewDepthImage, &rawOcclusionImage, &occlusionImage });
		prepassDepthImage = device.createImage({
			.extent = extent,
			.format = DEPTH_FORMAT,
			.usage  = ImageUsageFlags(ImageUsageFlagBits::eDepthAttachment)
		});

		const auto create_texture = [&](Format format) {
			return device.createImage({
				.extent = extent,
				.format = format,
				.usage  = ImageUsageFlags(ImageUsageFlagBits::eColorAttachment) | ImageUsageFlagBits::eSampled
			});
		};

		viewDepthImage    = create_texture(DEPTH_TEXTURE_FORMAT);
		rawOcclusionImage = create_texture(OCCLUSION_FORMAT);
		occlusionImage    = create_texture(OCCLUSION_FORMAT);
	}

	void Renderer::createShadowTargets()
	{
		shadowDepthImage = device.createImage({
			.extent = SHADOW_MAP_EXTENT,
			.format = DEPTH_FORMAT,
			.usage  = ImageUsageFlags(ImageUsageFlagBits::eDepthAttachment)
		});

		for (ImageHandle& shadow_map : shadowMaps)
			shadow_map = device.createImage({
				.extent = SHADOW_MAP_EXTENT,
				.format = DEPTH_TEXTURE_FORMAT,
				.usage  = ImageUsageFlags(ImageUsageFlagBits::eColorAttachment) | ImageUsageFlagBits::eSampled
			});
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

	void Renderer::drawOpaque(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum, const glm::vec3& eye, PipelineHandle Material::* pipeline)
	{
		materials.forEach([&](PoolHandle<Material> handle, const Material& material) {
			drawMeshes(frame, scene, scene_address, frustum, ToGpuHandle<MaterialTag>(handle), material.*pipeline, false);
		});
		drawInstances(frame, scene, scene_address, frustum, eye, instancedMaterial.*pipeline, fadingInstancedMaterial.*pipeline);
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

	void Renderer::drawInstances(Frame&           frame,
	                             Scene&           scene,
	                             uint64_t         scene_address,
	                             const Frustum&   frustum,
	                             const glm::vec3& eye,
	                             PipelineHandle   steady_pipeline,
	                             PipelineHandle   fading_pipeline)
	{
		if (steady_pipeline == PipelineHandle {} || fading_pipeline == PipelineHandle {})
			return;

		CommandList&              commands = frame.commandList();
		std::vector<InstanceDraw> fading_draws;
		const auto                record   = [&commands](const InstanceDraw& draw) {
			commands.pushConstants(draw.constants);
			commands.bindIndexBuffer(draw.mesh->indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexed(draw.mesh->indexCount, draw.instanceCount);
		};

		// Fading draws write sample masks, which costs early depth testing, so they are kept to batches that need them.
		commands.bindPipeline(steady_pipeline);
		scene.forEach<InstanceBatch>([&](Entity entity, const InstanceBatch& batch) {
			const InstanceSet* instances = meshes.get(batch.instances);
			if (!batch.visible || instances == nullptr)
				return;

			const glm::mat4 transform = scene.resolveWorldTransform(entity).matrix;
			const Bounds    bounds    = batch.bounds.transformed(transform);
			if (!frustum.intersects(bounds))
				return;

			for (const LodDraw& lod : SelectLodDraws(batch.lods, DistanceTo(bounds, eye), FarthestDistanceTo(bounds, eye)))
			{
				const Mesh* mesh = meshes.get(lod.mesh);
				if (mesh == nullptr)
					continue;

				const InstanceDraw draw = { { scene_address, instances->address, mesh->vertexAddress, transform, lod.fadeBands }, mesh, instances->count };
				if (lod.fading)
					fading_draws.push_back(draw);
				else
					record(draw);
			}
		});

		if (fading_draws.empty())
			return;

		commands.bindPipeline(fading_pipeline);
		std::ranges::for_each(fading_draws, record);
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
