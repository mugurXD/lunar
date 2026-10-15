#include <trok/world/road_routes.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
	constexpr float ARM_LENGTH         = 200.f;
	constexpr float POINT_SPACING      = 10.f;
	constexpr float CHECKPOINT_SPACING = 25.f;
	constexpr float LANE_TOLERANCE     = 0.05f;
	constexpr float NODE_REACH         = 1.f;
	constexpr float START_DISTANCE     = 50.f;

	enum Arm : uint32_t
	{
		eWest,
		eEast,
		eNorth
	};

	std::vector<glm::vec3> Line(const glm::vec3& from, const glm::vec3& to)
	{
		const int              pieces = static_cast<int>(glm::distance(from, to) / POINT_SPACING);
		std::vector<glm::vec3> points;
		for (int piece = 0; piece <= pieces; piece++)
			points.push_back(glm::mix(from, to, static_cast<float>(piece) / static_cast<float>(pieces)));

		return points;
	}

	trok::RoadNetwork Junction()
	{
		const glm::vec3 centre = { 0.f, 0.f, 0.f };
		return trok::RoadNetwork(
			{
				Line({ -ARM_LENGTH, 0.f, 0.f }, centre),
				Line(centre, { ARM_LENGTH, 0.f, 0.f }),
				Line(centre, { 0.f, 0.f, ARM_LENGTH })
			},
			trok::RoadClass { .name = "test:road" });
	}
}

TEST(RoadRoutes, RoadsWithoutIdsAreNumberedInOrder)
{
	const trok::RoadNetwork network = Junction();

	for (size_t road = 0; road < network.getRoadCount(); road++)
	{
		EXPECT_EQ(network.getRoadId(road), road);
		EXPECT_EQ(network.findRoad(network.getRoadId(road)), road);
	}

	EXPECT_FALSE(network.findRoad(static_cast<trok::RoadId>(network.getRoadCount())).has_value());
}

TEST(RoadRoutes, RoutesTurnOntoTheRoadLeadingToTheDestination)
{
	const trok::RoadNetwork network = Junction();
	const size_t            north   = *trok::NearestNode(network, { 0.f, 0.f, ARM_LENGTH }, NODE_REACH);

	const std::optional<std::vector<trok::RouteStep>> route = trok::FindRoute(network, { .road = eWest, .forward = true }, north);

	ASSERT_TRUE(route.has_value());
	EXPECT_EQ(*route, (std::vector<trok::RouteStep> { { .road = eWest, .forward = true }, { .road = eNorth, .forward = true } }));
}

TEST(RoadRoutes, RoutesNeverTurnBackOnTheSameRoad)
{
	const trok::RoadNetwork network = Junction();
	const size_t            west    = *trok::NearestNode(network, { -ARM_LENGTH, 0.f, 0.f }, NODE_REACH);

	EXPECT_FALSE(trok::FindRoute(network, { .road = eWest, .forward = true }, west).has_value()) << "the only way back is a u-turn";
	EXPECT_FALSE(trok::FindRoute(network, { .road = static_cast<trok::RoadId>(network.getRoadCount()), .forward = true }, west).has_value()) << "unknown roads cannot be routed from";
}

TEST(RoadRoutes, NearestNodesAreFoundWithinReach)
{
	const trok::RoadNetwork network = Junction();

	const std::optional<size_t> centre = trok::NearestNode(network, { POINT_SPACING, 0.f, 0.f }, ARM_LENGTH);
	ASSERT_TRUE(centre.has_value());
	EXPECT_EQ(network.getNodes()[*centre].ends.size(), 3u);
	EXPECT_FALSE(trok::NearestNode(network, { ARM_LENGTH, 0.f, ARM_LENGTH }, NODE_REACH).has_value());
}

TEST(RoadRoutes, CheckpointsKeepToTheRightLane)
{
	const trok::RoadNetwork network    = Junction();
	const trok::RoadClass&  road_class = network.getRoadClass();
	const float             lane       = trok::LaneOffset(road_class, true);

	const std::vector<trok::RouteStep> eastbound = { { .road = eWest, .forward = true }, { .road = eEast, .forward = true } };
	const std::vector<trok::RouteStep> westbound = { { .road = eEast, .forward = false }, { .road = eWest, .forward = false } };

	const std::vector<glm::vec3> east = trok::RouteCheckpoints(network, eastbound, 0.f, CHECKPOINT_SPACING);
	const std::vector<glm::vec3> west = trok::RouteCheckpoints(network, westbound, network.lengthOf(eEast), CHECKPOINT_SPACING);
	ASSERT_GT(east.size(), 2u);
	ASSERT_GT(west.size(), 2u);

	for (const glm::vec3& checkpoint : east)
	{
		EXPECT_NEAR(checkpoint.z, lane, LANE_TOLERANCE) << "heading east, the right lane is on the +z side";
		EXPECT_NEAR(checkpoint.y, road_class.surfaceOffset, LANE_TOLERANCE);
	}

	for (const glm::vec3& checkpoint : west)
		EXPECT_NEAR(checkpoint.z, -lane, LANE_TOLERANCE) << "heading west, the right lane is on the -z side";

	for (size_t checkpoint = 1; checkpoint < east.size(); checkpoint++)
	{
		EXPECT_GT(east[checkpoint].x, east[checkpoint - 1].x);
		EXPECT_LE(glm::distance(east[checkpoint - 1], east[checkpoint]), CHECKPOINT_SPACING);
	}
}

TEST(RoadRoutes, CheckpointsStartWhereTheCarIsAndLeaveJunctionsToTheLocalPlanner)
{
	const trok::RoadNetwork            network  = Junction();
	const std::vector<trok::RouteStep> route    = { { .road = eWest, .forward = true }, { .road = eNorth, .forward = true } };
	const std::vector<glm::vec3>       points   = trok::RouteCheckpoints(network, route, START_DISTANCE, CHECKPOINT_SPACING);
	const float                        junction = network.getNodes()[network.getNodeAt(eWest, false)].radius;

	ASSERT_FALSE(points.empty());
	EXPECT_NEAR(points.front().x, START_DISTANCE - ARM_LENGTH, LANE_TOLERANCE);
	EXPECT_NEAR(points.back().z, ARM_LENGTH, LANE_TOLERANCE);

	for (const glm::vec3& checkpoint : points)
		EXPECT_GE(glm::length(glm::vec2(checkpoint.x, checkpoint.z)), junction) << "the junction itself is crossed by the local planner";
}

TEST(RoadRoutes, UnknownRoadsHaveNoCheckpoints)
{
	const trok::RoadNetwork            network = Junction();
	const std::vector<trok::RouteStep> route   = { { .road = static_cast<trok::RoadId>(network.getRoadCount()), .forward = true } };

	EXPECT_TRUE(trok::RouteCheckpoints(network, route, 0.f, CHECKPOINT_SPACING).empty());
}
