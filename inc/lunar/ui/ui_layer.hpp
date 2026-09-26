#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/render/render_device.hpp>

#include <memory>

namespace Rml
{
	class Context;
}

namespace lunar::UI
{
	namespace imp
	{
		class UiRenderInterface;
		class UiSystemInterface;
	}

	class LUNAR_API UiLayer
	{
	public:
		UiLayer(Render::RenderDevice& device, Render::Format color_format) noexcept;
		~UiLayer() noexcept;

		UiLayer(const UiLayer&)            = delete;
		UiLayer& operator=(const UiLayer&) = delete;

		bool          loadFont(const Fs::Path& path, bool fallback = false);
		void          update(Render::Extent2D extent);
		void          record(Render::CommandList& commands, Render::Extent2D extent);
		void          setDebuggerVisible(bool visible);
		Rml::Context* getContext();

	private:
		std::unique_ptr<imp::UiRenderInterface> renderInterface;
		std::unique_ptr<imp::UiSystemInterface> systemInterface;
		Rml::Context*                           context = nullptr;
		Render::Extent2D                        extent  = {};
	};
}
