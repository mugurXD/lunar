#include <lunar/core/time.hpp>
#include <lunar/debug.hpp>
#include <algorithm>

namespace lunar::Time
{
	using namespace std::literals;
	using namespace std::chrono;

	TimeContext_T::TimeContext_T()
		: startTime(clock.now()),
		currentTime(startTime),
		lastTime(startTime),
		secondsTimer(startTime)
	{
	}

	void TimeContext_T::update()
	{
		lastTime    = currentTime;
		currentTime = clock.now();
		deltaTime   = std::min(currentTime - lastTime, MAX_DELTA_TIME);

		frameIndex++;
		framesThisSecond++;

		if (currentTime - secondsTimer >= 1s)
		{
			framesPerSecond  = framesThisSecond;
			secondsTimer     = currentTime;
			framesThisSecond = 0;
		}
	}

	float TimeContext_T::getDeltaTime() const
	{
		return duration<float>(deltaTime).count();
	}

	double TimeContext_T::getElapsedTime() const
	{
		return duration<double>(currentTime - startTime).count();
	}

	double TimeContext_T::getDeltaTimeMs() const
	{
		return duration<double, std::milli>(deltaTime).count();
	}

	double TimeContext_T::getCurrentTimeMs() const
	{
		return duration<double, std::milli>(currentTime - startTime).count();
	}

	int TimeContext_T::getFramerate() const
	{
		return framesPerSecond;
	}

	FrameTime TimeContext_T::getFrameTime() const
	{
		return FrameTime
		{
			.deltaTime   = getDeltaTime(),
			.elapsedTime = getElapsedTime(),
			.frameIndex  = frameIndex
		};
	}
}
