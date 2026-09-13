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

	void Engine::addSystem(SystemPhase phase, System system)
	{
		systemScheduler.addSystem(phase, std::move(system));
	}

	void Engine::runGameLoop()
	{
		while (window.isActive())
		{
			window.pollEvents();
			timeContext.update();
			systemScheduler.runFrame(activeScene, timeContext.getFrameTime());
			activeScene.flushDestroyedEntities();
			window.update();
		}
	}

	Engine::Engine(const EngineBuilder& builder)
		:
		timeContext(),
		renderContext(),
		window(builder.windowBuilder.build(renderContext)),
		activeScene(),
		appName(builder.appName),
		systemScheduler(builder.fixedTimestepSeconds)
	{
		systemScheduler.addSystem(SystemPhase::eFixedUpdate, [](Scene& scene, const FrameTime& frame_time) {
			scene.physicsUpdate(frame_time.deltaTime);
		});
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

	EngineBuilder& EngineBuilder::fixedTimestep(double seconds)
	{
		fixedTimestepSeconds = seconds;
		return *this;
	}
}
