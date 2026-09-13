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

	Time::TimeContext_T& Engine::getTimeContext()
	{
		return timeContext;
	}

	Render::Window_T& Engine::getWindow()
	{
		return window;
	}

	void Engine::runGameLoop()
	{
		while (window.isActive())
		{
			window.pollEvents();
			timeContext.update();
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

	Engine EngineBuilder::build() const
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
