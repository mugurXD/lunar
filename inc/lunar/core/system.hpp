#pragma once
#include <lunar/api.hpp>
#include <lunar/core/time.hpp>
#include <lunar/utils/collections.hpp>
#include <array>
#include <functional>

namespace lunar
{
	class LUNAR_API Scene;

	constexpr double DEFAULT_FIXED_TIMESTEP = 1.0 / 60.0;

	enum class LUNAR_API SystemPhase : size_t
	{
		eFixedUpdate = 0,
		eUpdate,
		eLateUpdate,
		ePreRender,
		eCount
	};

	using System = std::function<void(Scene&, const FrameTime&)>;

	class LUNAR_API SystemScheduler
	{
	public:
		SystemScheduler(double fixed_timestep) noexcept;
		~SystemScheduler()                     noexcept = default;

		void addSystem(SystemPhase phase, System system);
		void runFrame(Scene& scene, const FrameTime& frame_time);

	private:
		void runPhase(SystemPhase phase, Scene& scene, const FrameTime& frame_time) const;

		std::array<vector<System>, static_cast<size_t>(SystemPhase::eCount)> systems       = {};
		double                                                                fixedTimestep = DEFAULT_FIXED_TIMESTEP;
		double                                                                accumulator   = 0.0;
	};
}
