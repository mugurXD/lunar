#include "lunar/core/engine.hpp"
#include <lunar/render/components.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/debug.hpp>

namespace lunar
{
	namespace
	{
		constexpr std::string_view TOGGLE_DEBUG_MODE = "toggle_debug_mode";
		constexpr std::string_view DEBUG_MODE_KEY    = "keyboard.f3";
	}

	Scene& Engine::getActiveScene()
	{
		return activeScene;
	}

	Time::TimeContext_T& Engine::getTimeContext()
	{
		return timeContext;
	}

	Render::Window_T& Engine::getWindow()
	{
		return window;
	}

	Render::Renderer& Engine::getRenderer()
	{
		return renderer;
	}

	JobSystem& Engine::getJobSystem()
	{
		return jobSystem;
	}

	void Engine::addSystem(SystemPhase phase, System system)
	{
		systemScheduler.addSystem(phase, std::move(system));
	}

	bool Engine::isDebugMode() const
	{
		return debugMode;
	}

	void Engine::runGameLoop()
	{
		while (window.isActive())
		{
			window.pollEvents();
			timeContext.update();
			frameStats.record(timeContext.getFrameTime().deltaTime);
			jobSystem.processCompleted();

			if (imguiLayer.has_value())
			{
				imguiLayer->beginFrame();

				if (window.getActionDown(TOGGLE_DEBUG_MODE))
					debugMode = !debugMode;
			}

			systemScheduler.runFrame(activeScene, timeContext.getFrameTime());

			if (imguiLayer.has_value())
			{
				if (debugMode)
					frameStats.draw();

				imguiLayer->endFrame();
			}

			renderFrame();
			activeScene.flushDestroyedEntities();
			window.update();
		}
	}

	void Engine::renderFrame()
	{
		renderer.render(activeScene, imguiLayer.has_value() ? &*imguiLayer : nullptr);
	}

	Engine::Engine(const EngineBuilder& builder)
		:
		timeContext(),
		window(
			Render::WindowBuilder(builder.windowBuilder)
				.renderBackend(builder.backend)
				.build()
		),
		renderDevice(Render::CreateRenderDevice(
			Render::RenderDeviceSettings
			{
				.appName = builder.appName,
				.pWindow = builder.useWindow ? &window : nullptr,
				.backend = builder.backend
			}
		)),
		swapchain(builder.useWindow ? renderDevice->createSwapchain(window) : nullptr),
		renderer(*renderDevice, swapchain.get()),
		activeScene(),
		appName(builder.appName),
		systemScheduler(builder.fixedTimestepSeconds),
		jobSystem(JobSystem::defaultWorkerCount())
	{
		Input::SetGlobalHandler(window);

		if (swapchain != nullptr)
		{
			imguiLayer.emplace(*renderDevice, *swapchain, window);
			window.registerAction(TOGGLE_DEBUG_MODE, { { DEBUG_MODE_KEY } });
		}

		systemScheduler.addSystem(SystemPhase::eFixedUpdate, [](Scene& scene, const FrameTime& frame_time) {
			scene.physicsUpdate(frame_time.deltaTime);
		});

		systemScheduler.addSystem(SystemPhase::eFixedUpdate, Physics::CapturePhysicsPoses);
		systemScheduler.addSystem(SystemPhase::eUpdate,      Physics::ApplyPhysicsPoses);

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

	EngineBuilder& EngineBuilder::renderBackend(Render::Backend backend)
	{
		this->backend = backend;
		return *this;
	}

	EngineBuilder& EngineBuilder::noWindow()
	{
		useWindow = false;
		return *this;
	}
}
