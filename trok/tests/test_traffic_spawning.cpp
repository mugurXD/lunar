#include <trok/vehicle/traffic_spawning.hpp>
#include <trok/world/road_routes.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace
{
	constexpr float ROAD_HALF_LENGTH = 500.f;
	constexpr float POINT_SPACING    = 10.f;
	constexpr float NEAR_DISTANCE    = 80.f;
	constexpr float FOG_DISTANCE     = 300.f;
	constexpr float VIEW_ANGLE       = 75.f;
	constexpr float CLEAR_SIGHT      = 150.f;
	constexpr float DENSITY_RADIUS   = 100.f;
	constexpr float LANE_TOLERANCE   = 0.02f;
	constexpr float HALF_LENGTH      = 2.f;
	constexpr float CRUISE_SPEED     = 25.f;
	constexpr float SLOW_SPEED       = 10.f;
	constexpr float CLOSE_GAP        = 20.f;
	constexpr float TIGHT_GAP        = 10.f;
	constexpr float TOUCHING_GAP     = 1.f;
	constexpr float LEADER_X         = 60.f;
	constexpr float BEYOND_LEADER_X  = 120.f;
	constexpr float ONCOMING_X       = 20.f;
	constexpr float FOLLOWER_X       = -40.f;
	constexpr float METRES_PER_KM    = 1000.f;

	trok::RoadNetwork EastboundRoad()
	{
		const int              pieces = static_cast<int>(ROAD_HALF_LENGTH * 2.f / POINT_SPACING);
		std::vector<glm::vec3> points;
		for (int piece = 0; piece <= pieces; piece++)
			points.emplace_back(static_cast<float>(piece) * POINT_SPACING - ROAD_HALF_LENGTH, 0.f, 0.f);

		return trok::RoadNetwork(std::move(points), trok::RoadClass { .name = "test:road" });
	}
}

TEST(TrafficSpawning, VehiclesAreHiddenBehindTheViewerOrInTheFog)
{
	const trok::TrafficViewer viewer =
	{
		.position       = { 0.f, 0.f, 0.f },
		.forward        = { 0.f, 0.f, -1.f },
		.nearDistance   = NEAR_DISTANCE,
		.hiddenDistance = FOG_DISTANCE,
		.viewAngle      = VIEW_ANGLE
	};

	EXPECT_FALSE(trok::IsHidden(viewer, { 0.f, 0.f, -CLEAR_SIGHT })) << "in view and clear of the fog";
	EXPECT_TRUE(trok::IsHidden(viewer, { 0.f, 0.f, CLEAR_SIGHT })) << "behind the viewer";
	EXPECT_TRUE(trok::IsHidden(viewer, { 0.f, 0.f, -(FOG_DISTANCE + 1.f) })) << "inside the fog";
	EXPECT_FALSE(trok::IsHidden(viewer, { 0.f, 0.f, NEAR_DISTANCE * 0.5f })) << "too close to pop in, even behind";
}

TEST(TrafficSpawning, LaneLengthCountsEveryLaneNearby)
{
	const trok::RoadNetwork network = EastboundRoad();
	const float             lanes   = static_cast<float>(network.getRoadClass().lanes);

	EXPECT_NEAR(trok::LaneKilometresNear(network, { 0.f, 0.f, 0.f }, DENSITY_RADIUS), DENSITY_RADIUS * 2.f * lanes / METRES_PER_KM, LANE_TOLERANCE);
	EXPECT_FLOAT_EQ(trok::LaneKilometresNear(network, { 0.f, 0.f, ROAD_HALF_LENGTH }, DENSITY_RADIUS), 0.f);
}

TEST(TrafficSpawning, GapsOnlyCountTheNearestVehiclesInTheSameLane)
{
	const trok::RoadNetwork network = EastboundRoad();
	const float             lane    = trok::LaneOffset(network.getRoadClass(), true);
	const trok::LaneSpot    spot    = { .road = 0, .forward = true, .distance = ROAD_HALF_LENGTH };

	const std::vector<trok::LaneVehicle> vehicles =
	{
		{ .position = { LEADER_X, 0.f, lane },        .velocity = { SLOW_SPEED, 0.f, 0.f },   .halfLength = HALF_LENGTH },
		{ .position = { BEYOND_LEADER_X, 0.f, lane }, .velocity = { CRUISE_SPEED, 0.f, 0.f }, .halfLength = HALF_LENGTH },
		{ .position = { ONCOMING_X, 0.f, -lane },     .velocity = { -SLOW_SPEED, 0.f, 0.f },  .halfLength = HALF_LENGTH },
		{ .position = { FOLLOWER_X, 0.f, lane },      .velocity = { CRUISE_SPEED, 0.f, 0.f }, .halfLength = HALF_LENGTH }
	};

	const trok::LaneGaps gaps = trok::GapsAround(network, spot, vehicles, HALF_LENGTH);

	EXPECT_NEAR(gaps.ahead, LEADER_X - HALF_LENGTH * 2.f, LANE_TOLERANCE);
	EXPECT_NEAR(gaps.aheadSpeed, SLOW_SPEED, LANE_TOLERANCE);
	EXPECT_NEAR(gaps.behind, -FOLLOWER_X - HALF_LENGTH * 2.f, LANE_TOLERANCE) << "the oncoming car in the other lane is ignored";
	EXPECT_NEAR(gaps.behindSpeed, CRUISE_SPEED, LANE_TOLERANCE);
}

TEST(TrafficSpawning, SpawnsMatchTheTrafficOrWaitForAGap)
{
	const trok::DriverSettings settings;

	EXPECT_EQ(trok::SpawnSpeed({}, CRUISE_SPEED, settings), CRUISE_SPEED) << "an empty lane is entered at cruising speed";
	EXPECT_EQ(trok::SpawnSpeed({ .ahead = CLOSE_GAP, .aheadSpeed = SLOW_SPEED }, CRUISE_SPEED, settings), SLOW_SPEED) << "a slower car close ahead sets the pace";
	EXPECT_FALSE(trok::SpawnSpeed({ .behind = TIGHT_GAP, .behindSpeed = CRUISE_SPEED }, CRUISE_SPEED, settings).has_value()) << "the car behind would have to brake";
	EXPECT_FALSE(trok::SpawnSpeed({ .ahead = TOUCHING_GAP }, CRUISE_SPEED, settings).has_value()) << "no room in front";
}
