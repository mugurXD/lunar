#pragma once
#include <string_view>
#include <memory>

#include <lunar/core/time.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/core/system.hpp>
#include <lunar/core/common.hpp>
#include <lunar/render/window.hpp>
#include <lunar/render/context.hpp>

namespace lunar
{
	struct LUNAR_API EngineBuilder;

	class LUNAR_API Engine
	{
	public:
		~Engine() = default;

		Scene&                   getActiveScene();
		Render::RenderContext_T& getRenderContext();
		Render::Window_T&        getWindow();
		Time::TimeContext_T&     getTimeContext();
		void                     addSystem(SystemPhase phase, System system);
		void                     runGameLoop();

	private:
		Engine(const EngineBuilder& builder);
		friend class EngineBuilder;

		void renderFrame();

	private:
		Time::TimeContext_T     timeContext   = {};
		Render::RenderContext_T renderContext = {};
		Render::Window_T        window        = {};
		Scene                   activeScene   = {};
		std::string             appName       = {};
		SystemScheduler         systemScheduler;
	};

	struct LUNAR_API EngineBuilder
	{
	public:
		EngineBuilder()  noexcept = default;
		~EngineBuilder() noexcept = default;

		EngineBuilder& applicationName(const std::string_view& name);
		EngineBuilder& window(Render::WindowBuilder builder);
		EngineBuilder& fixedTimestep(double seconds);
		Engine build() const;

	private:
		Render::WindowBuilder windowBuilder        = {};
		std::string           appName              = "lunar";
		double                fixedTimestepSeconds = DEFAULT_FIXED_TIMESTEP;

		friend class Engine;
	};
}
