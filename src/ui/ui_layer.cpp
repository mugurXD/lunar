#include <lunar/ui/ui_layer.hpp>
#include <lunar/debug.hpp>

#include "ui_render_interface.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include <algorithm>
#include <system_error>

namespace lunar::UI
{
	namespace
	{
		constexpr const char* CONTEXT_NAME        = "main";
		constexpr float       REFERENCE_HEIGHT    = 1080.f;
		constexpr double      HOT_RELOAD_INTERVAL = 0.5;

		std::filesystem::file_time_type NewestChangeIn(const Fs::Path& directory)
		{
			std::error_code                 error;
			std::filesystem::file_time_type newest = {};

			for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory, error))
				if (entry.is_regular_file(error))
					newest = std::max(newest, entry.last_write_time(error));

			return newest;
		}
	}

	namespace imp
	{
		class UiSystemInterface final : public Rml::SystemInterface
		{
		public:
			bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
			{
				switch (type)
				{
				case Rml::Log::LT_ERROR:
				case Rml::Log::LT_ASSERT:
					DEBUG_ERROR("RmlUi: {}", message);
					break;
				case Rml::Log::LT_WARNING:
					DEBUG_WARN("RmlUi: {}", message);
					break;
				default:
					DEBUG_LOG("RmlUi: {}", message);
					break;
				}

				return true;
			}
		};
	}

	UiLayer::UiLayer(Render::RenderDevice& device, Render::Format color_format) noexcept
		: renderInterface(std::make_unique<imp::UiRenderInterface>(device, color_format)),
		systemInterface(std::make_unique<imp::UiSystemInterface>())
	{
		Rml::SetRenderInterface(renderInterface.get());
		Rml::SetSystemInterface(systemInterface.get());

		if (!Rml::Initialise())
		{
			DEBUG_ERROR("Failed to initialise RmlUi");
			return;
		}

		context = Rml::CreateContext(CONTEXT_NAME, { 1, 1 });
		if (context != nullptr)
			Rml::Debugger::Initialise(context);
	}

	UiLayer::~UiLayer() noexcept
	{
		Rml::Shutdown();
	}

	bool UiLayer::loadFont(const Fs::Path& path, bool fallback)
	{
		const bool loaded = Rml::LoadFontFace(path.string(), fallback);
		if (!loaded)
			DEBUG_ERROR("Failed to load font '{}'", path.string());

		return loaded;
	}

	Rml::ElementDocument* UiLayer::loadDocument(const Fs::Path& path)
	{
		if (context == nullptr)
			return nullptr;

		Rml::ElementDocument* document = context->LoadDocument(path.string());
		if (document == nullptr)
		{
			DEBUG_ERROR("Failed to load UI document '{}'", path.string());
			return nullptr;
		}

		documents.push_back({ path, document });
		lastChange = std::max(lastChange, NewestChangeIn(path.parent_path()));
		return document;
	}

	bool UiLayer::reloadChangedDocuments()
	{
		const std::filesystem::file_time_type newest = newestDocumentChange();
		if (context == nullptr || newest <= lastChange)
			return false;

		lastChange = newest;
		Rml::Factory::ClearStyleSheetCache();
		Rml::Factory::ClearTemplateCache();

		for (LoadedDocument& loaded : documents)
		{
			const bool visible = loaded.document != nullptr && loaded.document->IsVisible();
			if (loaded.document != nullptr)
				loaded.document->Close();

			loaded.document = context->LoadDocument(loaded.path.string());
			if (loaded.document != nullptr && visible)
				loaded.document->Show();
		}

		DEBUG_LOG("Reloaded {} UI documents", documents.size());
		return true;
	}

	void UiLayer::setHotReload(bool enabled)
	{
		hotReload = enabled;
	}

	std::filesystem::file_time_type UiLayer::newestDocumentChange() const
	{
		std::filesystem::file_time_type newest = {};
		for (const LoadedDocument& loaded : documents)
			newest = std::max(newest, NewestChangeIn(loaded.path.parent_path()));

		return newest;
	}

	void UiLayer::update(Render::Extent2D new_extent)
	{
		if (context == nullptr)
			return;

		const double now = systemInterface->GetElapsedTime();
		if (hotReload && now >= nextReloadCheck)
		{
			nextReloadCheck = now + HOT_RELOAD_INTERVAL;
			reloadChangedDocuments();
		}

		if (new_extent != extent && new_extent.width > 0 && new_extent.height > 0)
		{
			extent = new_extent;
			context->SetDimensions({ static_cast<int>(extent.width), static_cast<int>(extent.height) });
			context->SetDensityIndependentPixelRatio(static_cast<float>(extent.height) / REFERENCE_HEIGHT);
		}

		context->Update();
	}

	void UiLayer::record(Render::CommandList& commands, Render::Extent2D frame_extent)
	{
		if (context == nullptr)
			return;

		renderInterface->beginFrame(commands, frame_extent);
		context->Render();
		renderInterface->endFrame();
	}

	void UiLayer::setDebuggerVisible(bool visible)
	{
		Rml::Debugger::SetVisible(visible);
	}

	Rml::Context* UiLayer::getContext()
	{
		return context;
	}
}
