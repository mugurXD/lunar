#include "lunar/core/engine.hpp"
#include <lunar/debug.hpp>

namespace lunar
{
	Scene& Engine::getActiveScene()
	{
		return activeScene;
	}

	Render::RenderContext_T& Engine::getRenderContext()
	{
		return renderContext;
	}

	void Engine::runGameLoop()
	{
		while (window.isActive())
		{
			window.pollEvents();
			timeContext.update();
			DEBUG_LOG("FPS: {}, Current time: {}ms, Delta: {}ms", timeContext.getFramerate(), timeContext.getCurrentTimeMs(), timeContext.getDeltaTimeMs());
			window.update();
		}
	}

	Engine::Engine(const EngineBuilder& builder)
		: 
		appName(builder.appName),
		timeContext(),
		renderContext(),
		window(builder.windowBuilder.build(renderContext)),
		activeScene()
	{

	}

	Engine EngineBuilder::build()
	{
		return Engine(*this);
	}

	EngineBuilder& EngineBuilder::applicationName(const std::string_view& name)
	{
		appName = name;
		return *this;
	}

	EngineBuilder& EngineBuilder::window(Render::WindowBuilder builder)
	{
		windowBuilder = builder;
		return *this;
	}
}
