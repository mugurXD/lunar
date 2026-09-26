#pragma once
#include <lunar/render/render_device.hpp>
#include <lunar/core/handle.hpp>

#include <RmlUi/Core/RenderInterface.h>

#include <glm/glm.hpp>

namespace lunar::UI::imp
{
	class UiRenderInterface final : public Rml::RenderInterface
	{
	public:
		UiRenderInterface(Render::RenderDevice& device, Render::Format color_format) noexcept;
		~UiRenderInterface() noexcept override;

		UiRenderInterface(const UiRenderInterface&)            = delete;
		UiRenderInterface& operator=(const UiRenderInterface&) = delete;

		void beginFrame(Render::CommandList& commands, Render::Extent2D extent);
		void endFrame();

		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices)          override;
		void                        RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
		void                        ReleaseGeometry(Rml::CompiledGeometryHandle geometry)                                          override;
		Rml::TextureHandle          LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source)                               override;
		Rml::TextureHandle          GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions)                   override;
		void                        ReleaseTexture(Rml::TextureHandle texture)                                                     override;
		void                        EnableScissorRegion(bool enable)                                                               override;
		void                        SetScissorRegion(Rml::Rectanglei region)                                                       override;
		void                        SetTransform(const Rml::Matrix4f* transform)                                                   override;

	private:
		struct Geometry
		{
			Render::BufferHandle buffer        = {};
			uint64_t             vertexAddress = 0;
			size_t               indexOffset   = 0;
			uint32_t             indexCount    = 0;
		};

		void applyScissor();

		Render::RenderDevice&  device;
		Render::PipelineHandle pipeline       = {};
		bool                   linearOutput   = false;
		Pool<Geometry>         geometries;
		Render::CommandList*   commands       = nullptr;
		Render::Extent2D       extent         = {};
		glm::mat4              projection     = glm::mat4(1.f);
		glm::mat4              transform      = glm::mat4(1.f);
		bool                   scissorEnabled = false;
		Render::Rect2D         scissor        = {};
	};
}
