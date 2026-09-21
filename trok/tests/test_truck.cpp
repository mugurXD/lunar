#include <trok/vehicle/truck.hpp>
#include <trok/world/biome.hpp>
#include <trok/world/elevation.hpp>

#include <lunar/core/scene.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/physics/conversions.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

using namespace lunar::Physics;

namespace
{
	constexpr float STEP            = 1.f / 60.f;
	constexpr int   SETTLE_STEPS    = 240;
	constexpr int   DRIVE_STEPS     = 1800;
	constexpr int   BRAKE_STEPS     = 600;
	constexpr int   TURN_STEPS      = 600;
	constexpr float SPAWN_HEIGHT    = 2.f;
	constexpr float UPRIGHT         = 0.99f;
	constexpr float STILL_UPRIGHT   = 0.9f;
	constexpr float GOVERNOR_MARGIN = 0.5f;
	constexpr float STOPPED_SPEED   = 0.3f;
	constexpr float TURN_THROTTLE   = 0.4f;
	constexpr float MAX_COMPRESSION = 0.8f;

	const glm::vec3 GROUND_HALF_EXTENTS = { 2000.f, 1.f, 2000.f };
	const glm::vec3 GROUND_OFFSET       = { 0.f, -1.f, 0.f };
	const glm::vec3 UP                  = { 0.f, 1.f, 0.f };
	const glm::quat IDENTITY            = glm::quat(1.f, 0.f, 0.f, 0.f);

	std::filesystem::path DataFile(std::string_view name)
	{
		return std::filesystem::path(TROK_DATA_DIRECTORY) / name;
	}

	trok::TruckDefinition ShippedTruck()
	{
		return *Fs::LoadJson<trok::TruckDefinition>(DataFile("trucks/box_truck.json"));
	}

	struct TruckRig
	{
		lunar::Scene   scene;
		RigidBody      ground;
		RigidBody      chassis;
		RaycastVehicle vehicle;

		TruckRig(const trok::TruckDefinition& definition)
			: ground(scene, {}, IDENTITY, BodyType::eStatic),
			chassis(trok::CreateTruckBody(scene, definition, UP * SPAWN_HEIGHT, IDENTITY)),
			vehicle(definition.vehicle)
		{
			ground.addBox(GROUND_HALF_EXTENTS, GROUND_OFFSET, TERRAIN_CATEGORY);
		}

		void simulate(const VehicleInput& input, int steps)
		{
			for (int step = 0; step < steps; step++)
			{
				vehicle.update(chassis, input, STEP);
				scene.physicsUpdate(STEP);
			}
		}

		float uprightness()
		{
			return (ToGlm(chassis.getBody().getTransform().getOrientation()) * UP).y;
		}
	};
}

TEST(Truck, ShippedDataFilesLoad)
{
	EXPECT_TRUE(Fs::LoadJson<trok::BiomeLibrary>(DataFile("biomes.json")).has_value());
	EXPECT_TRUE(Fs::LoadJson<trok::ElevationCurve>(DataFile("elevation.json")).has_value());
	EXPECT_TRUE(Fs::LoadJson<trok::TruckDefinition>(DataFile("trucks/box_truck.json")).has_value());
}

TEST(Truck, DefinitionsRoundTripThroughJson)
{
	const trok::TruckDefinition definition = ShippedTruck();

	EXPECT_EQ(Fs::DeserializeJson<trok::TruckDefinition>(trok::TruckDefinition::Serialize(definition)), definition);
}

TEST(Truck, MalformedDefinitionsAreRejected)
{
	nlohmann::json weightless = trok::TruckDefinition::Serialize(ShippedTruck());
	weightless["body"]["mass"] = 0.f;

	nlohmann::json undriven = trok::TruckDefinition::Serialize(ShippedTruck());
	for (nlohmann::json& mount : undriven["wheels"]["mounts"])
		mount["driven"] = false;

	nlohmann::json missing_engine = trok::TruckDefinition::Serialize(ShippedTruck());
	missing_engine.erase("engine");

	nlohmann::json future_format = trok::TruckDefinition::Serialize(ShippedTruck());
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::TruckDefinition>(weightless).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::TruckDefinition>(undriven).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::TruckDefinition>(missing_engine).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::TruckDefinition>(future_format).has_value());
}

TEST(Truck, ShippedTruckSettlesDrivesAndStops)
{
	const trok::TruckDefinition definition = ShippedTruck();
	TruckRig                    rig(definition);

	rig.simulate({}, SETTLE_STEPS);
	EXPECT_TRUE(std::ranges::all_of(rig.vehicle.getWheels(), &WheelState::grounded));
	EXPECT_TRUE(std::ranges::all_of(rig.vehicle.getWheels(), [&](const WheelState& wheel) { return wheel.compression < definition.vehicle.restLength * MAX_COMPRESSION; }));
	EXPECT_GT(rig.uprightness(), UPRIGHT);

	rig.simulate({ .throttle = 1.f }, DRIVE_STEPS);
	EXPECT_NEAR(rig.vehicle.getForwardSpeed(), definition.vehicle.maxSpeed, GOVERNOR_MARGIN);

	rig.simulate({ .brake = 1.f }, BRAKE_STEPS);
	EXPECT_LT(std::abs(rig.vehicle.getForwardSpeed()), STOPPED_SPEED);
}

TEST(Truck, ShippedTruckStaysUprightAtFullLock)
{
	TruckRig rig(ShippedTruck());

	rig.simulate({}, SETTLE_STEPS);
	rig.simulate({ .throttle = TURN_THROTTLE, .steering = 1.f }, TURN_STEPS);

	EXPECT_GT(rig.uprightness(), STILL_UPRIGHT);
}
