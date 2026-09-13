#include <lunar/core/system.hpp>
#include <utility>

namespace lunar
{
	SystemScheduler::SystemScheduler(double fixed_timestep) noexcept
		: fixedTimestep(fixed_timestep)
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
		while (accumulator >= fixedTimestep)
		{
			runPhase(SystemPhase::eFixedUpdate, scene, fixed_frame_time);
			accumulator -= fixedTimestep;
		}

		FrameTime variable_frame_time      = frame_time;
		variable_frame_time.fixedDeltaTime = static_cast<float>(fixedTimestep);

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
