#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/render/render_device.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace Rml
{
	class Context;
	class ElementDocument;
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

		bool                  loadFont(const Fs::Path& path, bool fallback = false);
		Rml::ElementDocument* loadDocument(const Fs::Path& path);
		bool                  reloadChangedDocuments();
		void                  setHotReload(bool enabled);
		void                  update(Render::Extent2D extent);
		void                  record(Render::CommandList& commands, Render::Extent2D extent);
		void                  setDebuggerVisible(bool visible);
		Rml::Context*         getContext();

	private:
		struct LoadedDocument
		{
			Fs::Path              path     = {};
			Rml::ElementDocument* document = nullptr;
		};

		std::filesystem::file_time_type newestDocumentChange() const;

		std::unique_ptr<imp::UiRenderInterface> renderInterface;
		std::unique_ptr<imp::UiSystemInterface> systemInterface;
		Rml::Context*                           context         = nullptr;
		Render::Extent2D                        extent          = {};
		std::vector<LoadedDocument>             documents       = {};
		std::filesystem::file_time_type         lastChange      = {};
		bool                                    hotReload       = false;
		double                                  nextReloadCheck = 0.0;
	};
}
