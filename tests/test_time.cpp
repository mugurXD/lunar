#include <lunar/core/time.hpp>
#include <gtest/gtest.h>

namespace
{
	constexpr float MAX_DELTA_SECONDS = 0.25f;
}

TEST(TimeContext, FrameIndexCountsUpdates)
{
	lunar::Time::TimeContext_T time_context;

	EXPECT_EQ(time_context.getFrameTime().frameIndex, 0u);

	time_context.update();
	time_context.update();

	EXPECT_EQ(time_context.getFrameTime().frameIndex, 2u);
}

TEST(TimeContext, FrameTimeMatchesGetters)
{
	lunar::Time::TimeContext_T time_context;

	time_context.update();
	const lunar::FrameTime frame_time = time_context.getFrameTime();

	EXPECT_FLOAT_EQ(frame_time.deltaTime, time_context.getDeltaTime());
	EXPECT_DOUBLE_EQ(frame_time.elapsedTime, time_context.getElapsedTime());
}

TEST(TimeContext, DeltaTimeIsSmallAndClamped)
{
	lunar::Time::TimeContext_T time_context;

	time_context.update();

	EXPECT_GE(time_context.getDeltaTime(), 0.f);
	EXPECT_LE(time_context.getDeltaTime(), MAX_DELTA_SECONDS);
}
