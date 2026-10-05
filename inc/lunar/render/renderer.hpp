#pragma once
#include <lunar/api.hpp>
#include <lunar/render/frustum.hpp>
#include <lunar/render/render_device.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lunar
{
	class LUNAR_API Scene;
}

namespace lunar::UI
{
	class UiLayer;
}

namespace lunar::Render
{
	class ImGuiLayer;

	struct LUNAR_API MaterialDesc
	{
		std::string_view vertexShader   = {};
		std::string_view fragmentShader = {};
	};

	class LUNAR_API Renderer
	{
	public:
		Renderer(RenderDevice& device, Swapchain* swapchain, uint32_t samples = 1) noexcept;
		~Renderer() noexcept;

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		void           render(Scene& scene, UI::UiLayer* ui = nullptr, ImGuiLayer* debug_ui = nullptr);
		void           setSamples(uint32_t samples);
		uint32_t       getSamples() const;
		MaterialHandle createMaterial(const MaterialDesc& desc);
		MeshRegistry&  getMeshes();
		MeshHandle     getCubeMesh() const;
		void           drawLine(std::span<const glm::vec3> points, const glm::vec4& color = { 0.f, 0.f, 1.f, 1.f });

	private:
		struct Material
		{
			std::string       vertexShader        = {};
			std::string       fragmentShader      = {};
			PrimitiveTopology topology            = PrimitiveTopology::eTriangleList;
			PipelineHandle    opaquePipeline      = {};
			PipelineHandle    translucentPipeline = {};
		};

		void recreatePipelines(Material& material);
		void createPipelines(Material& material);
		void destroyPipelines(Material& material);
		void resizeTargets(Extent2D extent);
		void destroyTargets();
		void recordFrame(Frame& frame, Scene& scene, ImageHandle target, Extent2D extent);
		void recordOverlay(Frame& frame, ImageHandle target, Extent2D extent, UI::UiLayer* ui, ImGuiLayer* debug_ui);
		void drawMeshes(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum, MaterialHandle material, PipelineHandle pipeline, bool translucent);
		void drawLines(Frame& frame, uint64_t scene_address);

		RenderDevice&       device;
		Swapchain*          swapchain       = nullptr;
		MeshRegistry        meshes;
		MeshHandle          cubeMesh        = {};
		Pool<Material>      materials;
		Material            lineMaterial;
		std::vector<Vertex> lineVertices;
		MaterialHandle      defaultMaterial = {};
		ImageHandle         colorImage      = {};
		ImageHandle         depthImage      = {};
		uint32_t            sampleCount     = 0;
	};
}
