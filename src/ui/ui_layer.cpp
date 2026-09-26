#include <lunar/ui/ui_layer.hpp>
#include <lunar/debug.hpp>

#include "ui_render_interface.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

namespace lunar::UI
{
	namespace
	{
		constexpr const char* CONTEXT_NAME     = "main";
		constexpr float       REFERENCE_HEIGHT = 1080.f;
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

	void UiLayer::update(Render::Extent2D new_extent)
	{
		if (context == nullptr)
			return;

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
