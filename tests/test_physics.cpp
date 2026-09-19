#include <lunar/core/scene.hpp>
#include <lunar/physics/conversions.hpp>
#include <lunar/physics/raycast_vehicle.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <array>

using namespace lunar::Physics;

namespace
{
	constexpr float STEP             = 1.f / 60.f;
	constexpr int   SETTLE_STEPS     = 240;
	constexpr int   DRIVE_STEPS      = 300;
	constexpr int   BRAKE_STEPS      = 300;
	constexpr float GRAVITY          = 9.81f;
	constexpr float TRUCK_MASS       = 4000.f;
	constexpr float SPAWN_HEIGHT     = 1.5f;
	constexpr float REST_SPEED       = 0.05f;
	constexpr float UPRIGHT          = 0.99f;
	constexpr float LOAD_TOLERANCE   = 0.15f;
	constexpr float MIN_DRIVE_SPEED  = 5.f;
	constexpr float MIN_DISTANCE     = 10.f;
	constexpr float MAX_DRIFT        = 1.f;
	constexpr float STOPPED_SPEED    = 0.2f;
	constexpr float MIN_TURN         = 0.3f;
	constexpr float HALF_THROTTLE    = 0.5f;
	constexpr float HEIGHT_TOLERANCE = 0.01f;
	constexpr float RAY_START        = 100.f;

	const glm::vec3 GROUND_HALF_EXTENTS  = { 500.f, 1.f, 500.f };
	const glm::vec3 GROUND_OFFSET        = { 0.f, -1.f, 0.f };
	const glm::vec3 CHASSIS_HALF_EXTENTS = { 1.1f, 0.6f, 2.5f };
	const glm::vec3 CENTER_OF_MASS       = { 0.f, -0.3f, 0.f };
	const glm::vec3 FORWARD              = { 0.f, 0.f, -1.f };
	const glm::vec3 UP                   = { 0.f, 1.f, 0.f };
	const glm::quat IDENTITY             = glm::quat(1.f, 0.f, 0.f, 0.f);

	VehicleSettings TestTruck()
	{
		return VehicleSettings
		{
			.wheels =
			{
				{ .mountPoint = { -1.f, -0.5f, -2.f }, .radius = 0.5f, .steered = true },
				{ .mountPoint = {  1.f, -0.5f, -2.f }, .radius = 0.5f, .steered = true },
				{ .mountPoint = { -1.f, -0.5f,  2.f }, .radius = 0.5f, .driven  = true },
				{ .mountPoint = {  1.f, -0.5f,  2.f }, .radius = 0.5f, .driven  = true }
			},
			.restLength         = 0.5f,
			.stiffness          = 50000.f,
			.damping            = 8000.f,
			.driveForce         = 16000.f,
			.brakeForce         = 40000.f,
			.corneringStiffness = 20000.f,
			.tyreFriction       = 1.f,
			.rollingResistance  = 50.f,
			.dragCoefficient    = 1.f
		};
	}

	RigidBody CreateGround(lunar::Scene& scene)
	{
		RigidBody ground(scene, {}, IDENTITY, BodyType::eStatic);
		ground.addBox(GROUND_HALF_EXTENTS, GROUND_OFFSET, TERRAIN_CATEGORY);
		return ground;
	}

	RigidBody CreateChassis(lunar::Scene& scene)
	{
		RigidBody chassis(scene, UP * SPAWN_HEIGHT, IDENTITY, BodyType::eDynamic);
		chassis.addBox(CHASSIS_HALF_EXTENTS, {}, VEHICLE_CATEGORY);
		chassis.setMass(TRUCK_MASS, CENTER_OF_MASS);
		chassis.getBody().setIsAllowedToSleep(false);
		return chassis;
	}

	void Simulate(lunar::Scene& scene, RaycastVehicle& vehicle, RigidBody& chassis, const VehicleInput& input, int steps)
	{
		for (int step = 0; step < steps; step++)
		{
			vehicle.update(chassis, input, STEP);
			scene.physicsUpdate(STEP);
		}
	}

	glm::vec3 Position(RigidBody& body)
	{
		return ToGlm(body.getBody().getTransform().getPosition());
	}

	glm::quat Rotation(RigidBody& body)
	{
		return ToGlm(body.getBody().getTransform().getOrientation());
	}
}

TEST(RaycastVehicle, SettlesOnItsSuspension)
{
	lunar::Scene   scene;
	RigidBody      ground  = CreateGround(scene);
	RigidBody      chassis = CreateChassis(scene);
	RaycastVehicle vehicle(TestTruck());

	Simulate(scene, vehicle, chassis, {}, SETTLE_STEPS);

	const float expected_compression = TRUCK_MASS * GRAVITY / (vehicle.getSettings().stiffness * static_cast<float>(vehicle.getWheels().size()));
	for (const WheelState& wheel : vehicle.getWheels())
	{
		EXPECT_TRUE(wheel.grounded);
		EXPECT_NEAR(wheel.compression, expected_compression, expected_compression * LOAD_TOLERANCE);
	}

	EXPECT_LT(glm::length(ToGlm(chassis.getBody().getLinearVelocity())), REST_SPEED);
	EXPECT_GT((Rotation(chassis) * UP).y, UPRIGHT);
}

TEST(RaycastVehicle, ThrottleDrivesForwardAndBrakesStop)
{
	lunar::Scene   scene;
	RigidBody      ground  = CreateGround(scene);
	RigidBody      chassis = CreateChassis(scene);
	RaycastVehicle vehicle(TestTruck());

	Simulate(scene, vehicle, chassis, {}, SETTLE_STEPS);
	const glm::vec3 start = Position(chassis);

	Simulate(scene, vehicle, chassis, { .throttle = 1.f }, DRIVE_STEPS);
	const glm::vec3 travelled = Position(chassis) - start;

	EXPECT_GT(vehicle.getForwardSpeed(), MIN_DRIVE_SPEED);
	EXPECT_GT(glm::dot(travelled, FORWARD), MIN_DISTANCE);
	EXPECT_LT(std::abs(travelled.x), MAX_DRIFT);

	Simulate(scene, vehicle, chassis, { .brake = 1.f }, BRAKE_STEPS);
	EXPECT_LT(std::abs(vehicle.getForwardSpeed()), STOPPED_SPEED);
}

TEST(RaycastVehicle, SteeringRightTurnsTheVehicleRight)
{
	lunar::Scene   scene;
	RigidBody      ground  = CreateGround(scene);
	RigidBody      chassis = CreateChassis(scene);
	RaycastVehicle vehicle(TestTruck());

	Simulate(scene, vehicle, chassis, {}, SETTLE_STEPS);
	Simulate(scene, vehicle, chassis, { .throttle = HALF_THROTTLE, .steering = 1.f }, DRIVE_STEPS);

	EXPECT_GT((Rotation(chassis) * FORWARD).x, MIN_TURN);
	EXPECT_GT((Rotation(chassis) * UP).y, UPRIGHT);
}

TEST(RaycastVehicle, TheGovernorCapsTopSpeed)
{
	constexpr float GOVERNED_SPEED = 8.f;
	constexpr float OVERSHOOT      = 1.f;
	constexpr int   LONG_DRIVE     = 900;

	VehicleSettings settings = TestTruck();
	settings.maxSpeed        = GOVERNED_SPEED;

	lunar::Scene   scene;
	RigidBody      ground  = CreateGround(scene);
	RigidBody      chassis = CreateChassis(scene);
	RaycastVehicle vehicle(settings);

	Simulate(scene, vehicle, chassis, {}, SETTLE_STEPS);
	Simulate(scene, vehicle, chassis, { .throttle = 1.f }, LONG_DRIVE);

	EXPECT_LT(vehicle.getForwardSpeed(), GOVERNED_SPEED + OVERSHOOT);
	EXPECT_GT(vehicle.getForwardSpeed(), GOVERNED_SPEED - OVERSHOOT);
}

TEST(RaycastVehicle, WheelsWithoutGroundHaveNoGrip)
{
	lunar::Scene   scene;
	RigidBody      chassis = CreateChassis(scene);
	RaycastVehicle vehicle(TestTruck());

	Simulate(scene, vehicle, chassis, { .throttle = 1.f }, SETTLE_STEPS);

	EXPECT_TRUE(std::ranges::none_of(vehicle.getWheels(), &WheelState::grounded));
	EXPECT_LT(Position(chassis).y, SPAWN_HEIGHT);
}

TEST(RigidBody, HeightfieldsLineUpWithTheirSamples)
{
	constexpr uint32_t SAMPLES = 5;
	constexpr float    SPACING = 2.f;

	const glm::vec3 origin = { 10.f, 0.f, -20.f };
	const auto      height = [](uint32_t x, uint32_t z) { return static_cast<float>(x) * 1.5f + static_cast<float>(z) * 0.5f + 3.f; };

	std::array<float, SAMPLES * SAMPLES> heights = {};
	for (uint32_t z = 0; z < SAMPLES; z++)
		for (uint32_t x = 0; x < SAMPLES; x++)
			heights[z * SAMPLES + x] = height(x, z);

	lunar::Scene scene;
	RigidBody    field(scene, origin, IDENTITY, BodyType::eStatic);
	field.addHeightfield({ .heights = heights, .samplesPerSide = SAMPLES, .spacing = SPACING }, TERRAIN_CATEGORY);

	for (uint32_t z = 0; z < SAMPLES; z++)
	{
		for (uint32_t x = 0; x < SAMPLES; x++)
		{
			const glm::vec3   top = origin + glm::vec3(static_cast<float>(x) * SPACING, RAY_START, static_cast<float>(z) * SPACING);
			rp3d::RaycastInfo info;
			ASSERT_TRUE(field.getBody().raycast(rp3d::Ray(ToPhysics(top), ToPhysics(top - UP * RAY_START * 2.f)), info));
			EXPECT_NEAR(info.worldPoint.y, height(x, z), HEIGHT_TOLERANCE);
		}
	}
}
