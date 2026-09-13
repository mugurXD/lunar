#pragma once
#include <lunar/api.hpp>
#include <atomic>
#include <chrono>

namespace lunar::Time
{
	class LUNAR_API TimeContext_T
	{
	public:
		TimeContext_T();
		~TimeContext_T() = default;

		void   update();
		float  getDeltaTime()     const;
		double getElapsedTime()   const;
		double getDeltaTimeMs()   const;
		double getCurrentTimeMs() const;
		int    getFramerate()     const;

	private:
		static constexpr std::chrono::steady_clock::duration MAX_DELTA_TIME = std::chrono::milliseconds(250);

		std::chrono::steady_clock             clock            = {};
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point currentTime;
		std::chrono::steady_clock::time_point lastTime;
		std::chrono::steady_clock::duration   deltaTime        = {};
		std::chrono::steady_clock::time_point secondsTimer;
		int                                   framesThisSecond = 0;
		int                                   framesPerSecond  = 0;
	};
}
