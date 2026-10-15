#include <trok/world/drive_planner.hpp>
#include <trok/world/road_routes.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
	constexpr float ROAD_LENGTH    = 200.f;
	constexpr float POINT_SPACING  = 6.f;
	constexpr float TRAVEL         = 40.f;
	constexpr float LANE_TOLERANCE = 0.6f;
	constexpr float ROADSIDE       = 15.f;
	constexpr float WALL_HALF      = 5.f;
	constexpr float WALL_HEIGHT    = 100.f;
	constexpr float FAR_AWAY       = 1000.f;
	constexpr float SETTLED        = 20.f;
	constexpr float END_TOLERANCE  = 1e-3f;

	std::vector<glm::vec3> Line(const glm::vec3& from, const glm::vec3& to)
	{
		const int              pieces = static_cast<int>(glm::distance(from, to) / POINT_SPACING);
		std::vector<glm::vec3> points;
		for (int piece = 0; piece <= pieces; piece++)
			points.push_back(glm::mix(from, to, static_cast<float>(piece) / static_cast<float>(pieces)));

		return points;
	}

	trok::RoadNetwork EastboundRoad()
	{
		return trok::RoadNetwork(Line({ -ROAD_LENGTH, 0.f, 0.f }, { ROAD_LENGTH, 0.f, 0.f }), trok::RoadClass { .name = "test:road" });
	}

	trok::RoadNetwork Junction()
	{
		const glm::vec3 centre = { 0.f, 0.f, 0.f };
		return trok::RoadNetwork(
			{
				Line({ -ROAD_LENGTH, 0.f, 0.f }, centre),
				Line(centre, { ROAD_LENGTH, 0.f, 0.f }),
				Line(centre, { 0.f, 0.f, ROAD_LENGTH })
			},
			trok::RoadClass { .name = "test:road" });
	}

	std::optional<float> FlatGround(const glm::vec2&)
	{
		return 0.f;
	}

	std::optional<float> NoGround(const glm::vec2&)
	{
		return std::nullopt;
	}

	std::optional<float> Wall(const glm::vec2& position)
	{
		return std::abs(position.x) < WALL_HALF ? WALL_HEIGHT : 0.f;
	}
}

TEST(DrivePlanner, PathsAlongARoadKeepToTheRightLane)
{
	const trok::RoadNetwork network = EastboundRoad();
	const float             lane    = trok::LaneOffset(network.getRoadClass(), true);
	trok::DrivePlanner      planner(FlatGround);

	const std::vector<trok::DrivePoint> path = planner.plan(network, { -TRAVEL, 0.f, lane }, { TRAVEL, 0.f, lane });

	ASSERT_GT(path.size(), 2u);
	for (const trok::DrivePoint& point : path)
	{
		EXPECT_NEAR(point.position.z, lane, LANE_TOLERANCE);
		EXPECT_TRUE(point.onRoad);
	}
}

TEST(DrivePlanner, PathsLeaveTheWrongLaneAndKeepTheirEnds)
{
	const trok::RoadNetwork network = EastboundRoad();
	const float             lane    = trok::LaneOffset(network.getRoadClass(), true);
	trok::DrivePlanner      planner(FlatGround);

	const glm::vec3                     from = { -TRAVEL, 0.f, -lane };
	const glm::vec3                     to   = { TRAVEL, 0.f, lane };
	const std::vector<trok::DrivePoint> path = planner.plan(network, from, to);

	ASSERT_GT(path.size(), 2u);
	EXPECT_NEAR(path.front().position.x, from.x, END_TOLERANCE);
	EXPECT_NEAR(path.front().position.z, from.z, END_TOLERANCE);
	EXPECT_NEAR(glm::distance(path.back().position, to), 0.f, END_TOLERANCE);

	for (const trok::DrivePoint& point : path)
		if (point.position.x > from.x + SETTLED)
			EXPECT_NEAR(point.position.z, lane, LANE_TOLERANCE) << "eastbound traffic belongs on the +z side";
}

TEST(DrivePlanner, PathsFromOffRoadRejoinTheRoad)
{
	const trok::RoadNetwork network = EastboundRoad();
	const float             lane    = trok::LaneOffset(network.getRoadClass(), true);
	trok::DrivePlanner      planner(FlatGround);

	const std::vector<trok::DrivePoint> path = planner.plan(network, { -TRAVEL, 0.f, ROADSIDE }, { TRAVEL, 0.f, lane });

	ASSERT_GT(path.size(), 2u);
	EXPECT_FALSE(path.front().onRoad);
	EXPECT_TRUE(path.back().onRoad);
	EXPECT_TRUE(std::ranges::any_of(path, [](const trok::DrivePoint& point) { return !point.onRoad; }));
}

TEST(DrivePlanner, PathsTurnThroughJunctions)
{
	const trok::RoadNetwork network = Junction();
	const float             lane    = trok::LaneOffset(network.getRoadClass(), true);
	trok::DrivePlanner      planner(FlatGround);

	const std::vector<trok::DrivePoint> path = planner.plan(network, { -TRAVEL, 0.f, lane }, { -lane, 0.f, TRAVEL });

	ASSERT_GT(path.size(), 2u);
	EXPECT_TRUE(std::ranges::all_of(path, &trok::DrivePoint::onRoad)) << "turning does not need to leave the road";
}

TEST(DrivePlanner, PlansFailWhereTheGroundIsUnknownSteepOrTooFar)
{
	const trok::RoadNetwork network = EastboundRoad();
	trok::DrivePlanner      blind(NoGround);
	trok::DrivePlanner      walled(Wall);

	EXPECT_TRUE(blind.plan(network, { -TRAVEL, 0.f, ROAD_LENGTH }, { TRAVEL, 0.f, ROAD_LENGTH }).empty());
	EXPECT_TRUE(walled.plan(network, { -TRAVEL, 0.f, ROAD_LENGTH }, { TRAVEL, 0.f, ROAD_LENGTH }).empty());
	EXPECT_TRUE(walled.plan(network, { 0.f, 0.f, 0.f }, { FAR_AWAY, 0.f, 0.f }).empty());
}
