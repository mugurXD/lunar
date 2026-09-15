#pragma once
#include <lunar/api.hpp>
#include <lunar/render/render_device.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

namespace lunar
{
	class LUNAR_API Scene;
}

namespace lunar::Render
{
	class LUNAR_API Renderer
	{
	public:
		Renderer(RenderDevice& device, Swapchain* swapchain) noexcept;
		~Renderer() noexcept;

		Renderer(const Renderer&)            = delete;
		Renderer& operator=(const Renderer&) = delete;

		void          render(Scene& scene);
		MeshRegistry& getMeshes();
		MeshHandle    getCubeMesh() const;

	private:
		void resizeDepthImage(Extent2D extent);
		void recordFrame(CommandList& commands, Scene& scene, ImageHandle target, Extent2D extent);
		void drawMeshes(CommandList& commands, Scene& scene, const glm::mat4& view_projection);

		RenderDevice&  device;
		Swapchain*     swapchain    = nullptr;
		MeshRegistry   meshes;
		MeshHandle     cubeMesh     = {};
		PipelineHandle meshPipeline = {};
		ImageHandle    depthImage   = {};
	};
}
