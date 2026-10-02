#include <lunar/core/system.hpp>
#include <cmath>
#include <utility>

namespace lunar
{
	SystemScheduler::SystemScheduler(double fixed_timestep, uint32_t max_fixed_steps) noexcept
		: fixedTimestep(fixed_timestep),
		maxFixedSteps(max_fixed_steps)
	{
	}

	void SystemScheduler::addSystem(SystemPhase phase, System system)
	{
		systems[static_cast<size_t>(phase)].push_back(std::move(system));
	}

	void SystemScheduler::runFrame(Scene& scene, const FrameTime& frame_time)
	{
		FrameTime fixed_frame_time      = frame_time;
		fixed_frame_time.deltaTime      = static_cast<float>(fixedTimestep);
		fixed_frame_time.fixedDeltaTime = static_cast<float>(fixedTimestep);

		accumulator += frame_time.deltaTime;
		for (uint32_t step = 0; step < maxFixedSteps && accumulator >= fixedTimestep; step++)
		{
			runPhase(SystemPhase::eFixedUpdate, scene, fixed_frame_time);
			accumulator -= fixedTimestep;
		}

		accumulator = std::fmod(accumulator, fixedTimestep);

		FrameTime variable_frame_time      = frame_time;
		variable_frame_time.fixedDeltaTime = static_cast<float>(fixedTimestep);
		variable_frame_time.fixedAlpha     = static_cast<float>(accumulator / fixedTimestep);

		runPhase(SystemPhase::eUpdate,     scene, variable_frame_time);
		runPhase(SystemPhase::eLateUpdate, scene, variable_frame_time);
		runPhase(SystemPhase::ePreRender,  scene, variable_frame_time);
	}

	void SystemScheduler::runPhase(SystemPhase phase, Scene& scene, const FrameTime& frame_time) const
	{
		for (const System& system : systems[static_cast<size_t>(phase)])
			system(scene, frame_time);
	}
}
