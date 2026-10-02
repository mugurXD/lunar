#pragma once
#include <lunar/api.hpp>
#include <lunar/render/frustum.hpp>
#include <lunar/render/render_device.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <string>
#include <string_view>

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

	private:
		struct Material
		{
			std::string    vertexShader        = {};
			std::string    fragmentShader      = {};
			PipelineHandle opaquePipeline      = {};
			PipelineHandle translucentPipeline = {};
		};

		void createPipelines(Material& material);
		void destroyPipelines(Material& material);
		void resizeTargets(Extent2D extent);
		void destroyTargets();
		void recordFrame(Frame& frame, Scene& scene, ImageHandle target, Extent2D extent);
		void recordOverlay(Frame& frame, ImageHandle target, Extent2D extent, UI::UiLayer* ui, ImGuiLayer* debug_ui);
		void drawMeshes(Frame& frame, Scene& scene, uint64_t scene_address, const Frustum& frustum, MaterialHandle material, PipelineHandle pipeline, bool translucent);

		RenderDevice&  device;
		Swapchain*     swapchain       = nullptr;
		MeshRegistry   meshes;
		MeshHandle     cubeMesh        = {};
		Pool<Material> materials;
		MaterialHandle defaultMaterial = {};
		ImageHandle    colorImage      = {};
		ImageHandle    depthImage      = {};
		uint32_t       sampleCount     = 0;
	};
}
