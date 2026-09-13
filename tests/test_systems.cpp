#include <lunar/core/system.hpp>
#include <lunar/core/scene.hpp>
#include <gtest/gtest.h>
#include <string>

namespace
{
	constexpr double FIXED_TIMESTEP  = 0.5;
	constexpr float  SHORT_FRAME     = 0.1f;
	constexpr float  LONG_FRAME      = 1.25f;
	constexpr float  REMAINDER_FRAME = 0.25f;

	lunar::FrameTime MakeFrameTime(float delta_time)
	{
		return lunar::FrameTime{ .deltaTime = delta_time };
	}

	lunar::System RecordInto(std::string& record, char marker)
	{
		return [&record, marker](lunar::Scene&, const lunar::FrameTime&) { record += marker; };
	}
}

TEST(SystemScheduler, PhasesRunInOrder)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	std::string            record;

	scheduler.addSystem(lunar::SystemPhase::ePreRender,   RecordInto(record, 'P'));
	scheduler.addSystem(lunar::SystemPhase::eLateUpdate,  RecordInto(record, 'L'));
	scheduler.addSystem(lunar::SystemPhase::eUpdate,      RecordInto(record, 'U'));
	scheduler.addSystem(lunar::SystemPhase::eFixedUpdate, RecordInto(record, 'F'));

	scheduler.runFrame(scene, MakeFrameTime(static_cast<float>(FIXED_TIMESTEP)));

	EXPECT_EQ(record, "FULP");
}

TEST(SystemScheduler, SystemsInPhaseRunInRegistrationOrder)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	std::string            record;

	scheduler.addSystem(lunar::SystemPhase::eUpdate, RecordInto(record, '1'));
	scheduler.addSystem(lunar::SystemPhase::eUpdate, RecordInto(record, '2'));
	scheduler.addSystem(lunar::SystemPhase::eUpdate, RecordInto(record, '3'));

	scheduler.runFrame(scene, MakeFrameTime(SHORT_FRAME));

	EXPECT_EQ(record, "123");
}

TEST(SystemScheduler, FixedUpdateRunsOncePerElapsedTimestep)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	int                    fixed_runs = 0;

	scheduler.addSystem(lunar::SystemPhase::eFixedUpdate, [&](lunar::Scene&, const lunar::FrameTime&) { fixed_runs++; });

	scheduler.runFrame(scene, MakeFrameTime(LONG_FRAME));
	EXPECT_EQ(fixed_runs, 2);

	scheduler.runFrame(scene, MakeFrameTime(REMAINDER_FRAME));
	EXPECT_EQ(fixed_runs, 3);
}

TEST(SystemScheduler, FixedUpdateIsSkippedForShortFramesButUpdateStillRuns)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	int                    fixed_runs  = 0;
	int                    update_runs = 0;

	scheduler.addSystem(lunar::SystemPhase::eFixedUpdate, [&](lunar::Scene&, const lunar::FrameTime&) { fixed_runs++; });
	scheduler.addSystem(lunar::SystemPhase::eUpdate,      [&](lunar::Scene&, const lunar::FrameTime&) { update_runs++; });

	scheduler.runFrame(scene, MakeFrameTime(SHORT_FRAME));

	EXPECT_EQ(fixed_runs, 0);
	EXPECT_EQ(update_runs, 1);
}

TEST(SystemScheduler, FixedUpdateReceivesFixedTimestepAsDelta)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	float                  received_delta = 0.f;
	float                  received_fixed = 0.f;

	scheduler.addSystem(lunar::SystemPhase::eFixedUpdate, [&](lunar::Scene&, const lunar::FrameTime& frame_time) {
		received_delta = frame_time.deltaTime;
		received_fixed = frame_time.fixedDeltaTime;
	});

	scheduler.runFrame(scene, MakeFrameTime(LONG_FRAME));

	EXPECT_FLOAT_EQ(received_delta, static_cast<float>(FIXED_TIMESTEP));
	EXPECT_FLOAT_EQ(received_fixed, static_cast<float>(FIXED_TIMESTEP));
}

TEST(SystemScheduler, VariablePhasesReceiveFrameDelta)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);
	lunar::FrameTime       received = {};

	scheduler.addSystem(lunar::SystemPhase::eUpdate, [&](lunar::Scene&, const lunar::FrameTime& frame_time) {
		received = frame_time;
	});

	scheduler.runFrame(scene, lunar::FrameTime{ .deltaTime = SHORT_FRAME, .elapsedTime = 3.0, .frameIndex = 7 });

	EXPECT_FLOAT_EQ(received.deltaTime, SHORT_FRAME);
	EXPECT_FLOAT_EQ(received.fixedDeltaTime, static_cast<float>(FIXED_TIMESTEP));
	EXPECT_DOUBLE_EQ(received.elapsedTime, 3.0);
	EXPECT_EQ(received.frameIndex, 7u);
}

TEST(SystemScheduler, SystemsOperateOnTheGivenScene)
{
	lunar::Scene           scene;
	lunar::SystemScheduler scheduler(FIXED_TIMESTEP);

	scheduler.addSystem(lunar::SystemPhase::eUpdate, [](lunar::Scene& target, const lunar::FrameTime&) {
		target.createGameObject("Spawned");
	});

	scheduler.runFrame(scene, MakeFrameTime(SHORT_FRAME));

	EXPECT_FALSE(scene.getGameObject("Spawned") == nullptr);
}
