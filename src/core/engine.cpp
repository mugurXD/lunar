#include "lunar/core/engine.hpp"
#include <lunar/render/components.hpp>
#include <lunar/debug.hpp>

namespace lunar
{
	const glm::vec4 CLEAR_COLOR = { 0.1f, 0.1f, 0.12f, 1.f };

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
			renderFrame();
			activeScene.flushDestroyedEntities();
			window.update();
		}
	}

	void Engine::renderFrame()
	{
		renderContext.begin(&window);
		renderContext.clear(CLEAR_COLOR.r, CLEAR_COLOR.g, CLEAR_COLOR.b, CLEAR_COLOR.a);

		const Camera* camera = activeScene.getMainCamera();
		if (camera != nullptr)
		{
			renderContext.useCamera(camera);

			const Render::GpuCubemap environment = activeScene.getEnvironment();
			if (environment != nullptr)
				renderContext.draw(environment);

			renderContext.draw(activeScene);
		}

		renderContext.end();
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
		Input::SetGlobalHandler(window);

		systemScheduler.addSystem(SystemPhase::eFixedUpdate, [](Scene& scene, const FrameTime& frame_time) {
			scene.physicsUpdate(frame_time.deltaTime);
		});

		systemScheduler.addSystem(SystemPhase::eUpdate, [](Scene& scene, const FrameTime& frame_time) {
			scene.updateBehaviours(frame_time);
		});

		systemScheduler.addSystem(SystemPhase::eLateUpdate, UpdateCameras);
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
