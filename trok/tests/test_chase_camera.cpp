#include <trok/vehicle/chase_camera.hpp>
#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace
{
	constexpr float STEP          = 1.f / 60.f;
	constexpr int   LONG_UPDATE   = 600;
	constexpr int   SHORT_UPDATE  = 30;
	constexpr float QUARTER_TURN  = 90.f;
	constexpr float DRIVING_SPEED = 10.f;
	constexpr float LARGE_INPUT   = 1000.f;
	constexpr float TOLERANCE     = 0.05f;

	const glm::vec3 WORLD_UP        = { 0.f, 1.f, 0.f };
	const glm::vec3 CAMERA_FORWARD  = { 0.f, 0.f, -1.f };
	const glm::vec3 TARGET_POSITION = { 10.f, 5.f, -3.f };

	const trok::ChaseCameraSettings SETTINGS = {};

	lunar::Transform Target()
	{
		return lunar::Transform { .position = TARGET_POSITION };
	}

	glm::vec3 Focus(const lunar::Transform& target)
	{
		return target.position + WORLD_UP * SETTINGS.lookHeight;
	}

	void Update(trok::ChaseCamera& chase, lunar::Transform& camera, const lunar::Transform& target, float speed, int steps)
	{
		for (int step = 0; step < steps; step++)
			chase.update(camera, target, speed, STEP);
	}

	void ExpectLooksAtFocus(const lunar::Transform& camera, const lunar::Transform& target)
	{
		const glm::vec3 forward  = camera.rotation * CAMERA_FORWARD;
		const glm::vec3 expected = glm::normalize(Focus(target) - camera.position);

		EXPECT_NEAR(glm::dot(forward, expected), 1.f, TOLERANCE);
	}
}

TEST(ChaseCamera, SnapsBehindAndAboveTheTarget)
{
	const lunar::Transform target = Target();
	lunar::Transform       camera;
	trok::ChaseCamera      chase(SETTINGS);

	chase.snap(camera, target);

	EXPECT_NEAR(glm::distance(camera.position, Focus(target)), SETTINGS.distance, TOLERANCE);
	EXPECT_GT(camera.position.z, target.position.z);
	EXPECT_GT(camera.position.y, Focus(target).y);
	ExpectLooksAtFocus(camera, target);
}

TEST(ChaseCamera, OrbitingMovesAroundTheTargetAtTheSameDistance)
{
	const lunar::Transform target = Target();
	lunar::Transform       camera;
	trok::ChaseCamera      chase(SETTINGS);

	chase.snap(camera, target);
	chase.orbit({ QUARTER_TURN, 0.f }, 0.f);
	Update(chase, camera, target, 0.f, 1);

	EXPECT_LT(camera.position.x, target.position.x);
	EXPECT_NEAR(glm::distance(camera.position, Focus(target)), SETTINGS.distance, TOLERANCE);
	ExpectLooksAtFocus(camera, target);
}

TEST(ChaseCamera, ZoomAndPitchStayWithinTheirLimits)
{
	const lunar::Transform target = Target();
	lunar::Transform       camera;
	trok::ChaseCamera      chase(SETTINGS);

	chase.orbit({}, LARGE_INPUT);
	EXPECT_FLOAT_EQ(chase.getDistance(), SETTINGS.minDistance);

	chase.orbit({}, -LARGE_INPUT);
	EXPECT_FLOAT_EQ(chase.getDistance(), SETTINGS.maxDistance);

	chase.orbit({ 0.f, -LARGE_INPUT }, 0.f);
	chase.snap(camera, target);

	const float elevation = glm::degrees(std::asin((camera.position.y - Focus(target).y) / chase.getDistance()));
	EXPECT_NEAR(elevation, SETTINGS.maxPitch, TOLERANCE);
}

TEST(ChaseCamera, RecentersBehindOnlyWhileDriving)
{
	const lunar::Transform target = Target();
	lunar::Transform       parked_camera;
	lunar::Transform       driving_camera;
	trok::ChaseCamera      parked(SETTINGS);
	trok::ChaseCamera      driving(SETTINGS);

	for (trok::ChaseCamera* chase : { &parked, &driving })
		chase->orbit({ QUARTER_TURN, 0.f }, 0.f);

	Update(parked,  parked_camera,  target, 0.f,           LONG_UPDATE);
	Update(driving, driving_camera, target, DRIVING_SPEED, LONG_UPDATE);

	EXPECT_LT(parked_camera.position.x, target.position.x - SETTINGS.distance * 0.5f);
	EXPECT_NEAR(driving_camera.position.x, target.position.x, SETTINGS.distance * TOLERANCE);
	EXPECT_GT(driving_camera.position.z, target.position.z);
}

TEST(ChaseCamera, RecenteringWaitsForTheDelay)
{
	const lunar::Transform target = Target();
	lunar::Transform       camera;
	trok::ChaseCamera      chase(SETTINGS);

	chase.snap(camera, target);
	chase.orbit({ QUARTER_TURN, 0.f }, 0.f);
	Update(chase, camera, target, DRIVING_SPEED, SHORT_UPDATE);

	EXPECT_LT(camera.position.x, target.position.x - SETTINGS.distance * 0.5f);
}

TEST(ChaseCamera, FollowsTheTargetsHeading)
{
	lunar::Transform  target = Target();
	lunar::Transform  camera;
	trok::ChaseCamera chase(SETTINGS);

	chase.snap(camera, target);
	target.rotation = glm::angleAxis(glm::radians(-QUARTER_TURN), WORLD_UP);
	Update(chase, camera, target, 0.f, LONG_UPDATE);

	EXPECT_LT(camera.position.x, target.position.x - SETTINGS.distance * 0.5f);
	EXPECT_NEAR(camera.position.z, target.position.z, SETTINGS.distance * TOLERANCE);
	ExpectLooksAtFocus(camera, target);
}
