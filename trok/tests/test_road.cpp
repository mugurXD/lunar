#include <trok/world/road.hpp>
#include <trok/world/road_planner.hpp>

#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TOLERANCE       = 0.5f;
	constexpr float GRADE_TOLERANCE = 1.05f;
	constexpr float WALL_HEIGHT     = 400.f;
	constexpr float WALL_START      = 400.f;
	constexpr float WALL_END        = 600.f;
	constexpr float PASS_CENTRE     = 300.f;
	constexpr float PASS_HALF_WIDTH = 80.f;
	constexpr float ROAD_LENGTH     = 1000.f;
	constexpr float HILL_AMPLITUDE  = 60.f;
	constexpr float HILL_WAVELENGTH = 300.f;
	constexpr float ROAD_HEIGHT     = 10.f;
	constexpr float FAR_AWAY        = 500.f;

	constexpr float HILL_HEIGHT       = 120.f;
	constexpr float HILL_RADIUS       = 300.f;
	constexpr float CUT_TOLERANCE     = 1.2f;
	constexpr float REFINED_CUT_LIMIT = 3.f;
	constexpr float SLOPE_GRADIENT    = 0.35f;
	constexpr float VERTEX_SPACING    = 2.f;
	constexpr float LOOKUP_SPACING    = 6.f;
	constexpr float LOOKUP_WANDER     = 40.f;
	constexpr float LOOKUP_RECT_SIZE  = 64.f;

	const glm::vec2 START = { 0.f, 0.f };
	const glm::vec2 END   = { ROAD_LENGTH, 0.f };

	trok::RoadClass TestRoadClass()
	{
		return trok::RoadClass { .name = "test:road" };
	}

	float FlatGround(double, double)
	{
		return 0.f;
	}

	float WallWithAPass(double x, double z)
	{
		const bool inside_wall = x >= WALL_START && x <= WALL_END;
		const bool inside_pass = std::abs(z - PASS_CENTRE) <= PASS_HALF_WIDTH;
		return inside_wall && !inside_pass ? WALL_HEIGHT : 0.f;
	}

	float ConeHill(double x, double z)
	{
		const float distance = glm::distance(glm::vec2(static_cast<float>(x), static_cast<float>(z)), glm::vec2(ROAD_LENGTH * 0.5f, 0.f));
		return std::max(0.f, HILL_HEIGHT * (1.f - distance / HILL_RADIUS));
	}

	float SteepSlope(double x, double)
	{
		return static_cast<float>(x) * SLOPE_GRADIENT;
	}

	float RollingHills(double x, double z)
	{
		return HILL_AMPLITUDE * static_cast<float>(std::sin(x / HILL_WAVELENGTH) * std::cos(z / HILL_WAVELENGTH));
	}
}

TEST(RoadClass, RoundTripsThroughJson)
{
	const trok::RoadClass road_class = TestRoadClass();

	EXPECT_EQ(Fs::DeserializeJson<trok::RoadClass>(trok::RoadClass::Serialize(road_class)), road_class);
	EXPECT_FLOAT_EQ(road_class.halfWidth(), road_class.laneWidth + road_class.shoulderWidth);
}

TEST(RoadClass, MalformedClassesAreRejected)
{
	nlohmann::json without_lanes = trok::RoadClass::Serialize(TestRoadClass());
	without_lanes["lanes"] = 0;

	nlohmann::json future_format = trok::RoadClass::Serialize(TestRoadClass());
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::RoadClass>(without_lanes).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RoadClass>(future_format).has_value());
}

TEST(RoadPlanner, ConnectsBothEndpointsOnFlatGround)
{
	const std::optional<std::vector<glm::vec3>> road = trok::PlanRoad(START, END, TestRoadClass(), FlatGround);

	ASSERT_TRUE(road.has_value());
	ASSERT_GT(road->size(), 2u);
	EXPECT_NEAR(glm::distance(glm::vec2(road->front().x, road->front().z), START), 0.f, TOLERANCE);
	EXPECT_NEAR(glm::distance(glm::vec2(road->back().x, road->back().z), END), 0.f, TOLERANCE);

	for (const glm::vec3& point : *road)
		EXPECT_NEAR(point.y, 0.f, TOLERANCE);
}

TEST(RoadPlanner, RoutesThroughAPassInsteadOfOverTheWall)
{
	const std::optional<std::vector<glm::vec3>> road = trok::PlanRoad(START, END, TestRoadClass(), WallWithAPass);

	ASSERT_TRUE(road.has_value());

	for (const glm::vec3& point : *road)
		if (point.x >= WALL_START && point.x <= WALL_END)
			EXPECT_LT(std::abs(point.z - PASS_CENTRE), PASS_HALF_WIDTH) << "the road crosses the wall at x = " << point.x;
}

TEST(RoadPlanner, ProfileRespectsTheMaximumGrade)
{
	const trok::RoadClass                       road_class = TestRoadClass();
	const std::optional<std::vector<glm::vec3>> road       = trok::PlanRoad(START, END, road_class, RollingHills);

	ASSERT_TRUE(road.has_value());

	for (size_t index = 1; index < road->size(); index++)
	{
		const glm::vec3& previous = (*road)[index - 1];
		const glm::vec3& current  = (*road)[index];
		const float      run      = glm::distance(glm::vec2(previous.x, previous.z), glm::vec2(current.x, current.z));

		EXPECT_LE(std::abs(current.y - previous.y), road_class.maxGrade * run * GRADE_TOLERANCE);
	}
}

TEST(RoadNetwork, GradingCutsIntoTheTerrainAndBlendsBack)
{
	const trok::RoadClass road_class = TestRoadClass();
	const trok::RoadNetwork network({ { 0.f, 0.f, 0.f }, { ROAD_LENGTH, 0.f, 0.f } }, road_class);

	const float centre     = network.gradedHeight(ROAD_LENGTH * 0.5, 0.0, ROAD_HEIGHT);
	const float edge       = network.gradedHeight(ROAD_LENGTH * 0.5, road_class.halfWidth(), ROAD_HEIGHT);
	const float embankment = network.gradedHeight(ROAD_LENGTH * 0.5, road_class.halfWidth() + ROAD_HEIGHT, ROAD_HEIGHT);
	const float untouched  = network.gradedHeight(ROAD_LENGTH * 0.5, FAR_AWAY, ROAD_HEIGHT);

	EXPECT_FLOAT_EQ(centre, 0.f);
	EXPECT_FLOAT_EQ(edge,   0.f);
	EXPECT_GT(embankment, 0.f);
	EXPECT_LT(embankment, ROAD_HEIGHT);
	EXPECT_FLOAT_EQ(untouched, ROAD_HEIGHT);
}

TEST(RoadNetwork, FillIsLeftToTheRoadGeometry)
{
	const trok::RoadNetwork network({ { 0.f, ROAD_HEIGHT, 0.f }, { ROAD_LENGTH, ROAD_HEIGHT, 0.f } }, TestRoadClass());

	EXPECT_FLOAT_EQ(network.gradedHeight(ROAD_LENGTH * 0.5, 0.0, 0.f), 0.f);
}

TEST(RoadPlanner, StaysWithinWhatGradingCanExcavate)
{
	const trok::RoadClass                       road_class = TestRoadClass();
	const std::optional<std::vector<glm::vec3>> road       = trok::PlanRoad(START, END, road_class, ConeHill);

	ASSERT_TRUE(road.has_value());

	const float deepest_cut = road_class.maxFillHeight * CUT_TOLERANCE;
	for (const glm::vec3& point : *road)
		EXPECT_LE(std::abs(point.y - ConeHill(point.x, point.z)), deepest_cut) << "the road is buried at (" << point.x << ", " << point.z << ")";
}

TEST(RoadPlanner, RefinementFollowsTheGroundAndEasesTheCurves)
{
	const trok::RoadClass                       road_class = TestRoadClass();
	const std::optional<std::vector<glm::vec3>> road       = trok::PlanRoad(START, END, road_class, RollingHills);

	ASSERT_TRUE(road.has_value());
	ASSERT_GT(road->size(), 2u);

	float deepest  = 0.f;
	float sharpest = 0.f;

	for (size_t index = 1; index + 1 < road->size(); index++)
	{
		const glm::vec2 previous = { (*road)[index - 1].x, (*road)[index - 1].z };
		const glm::vec2 point    = { (*road)[index].x,     (*road)[index].z };
		const glm::vec2 next     = { (*road)[index + 1].x, (*road)[index + 1].z };
		const glm::vec2 incoming = point - previous;
		const glm::vec2 outgoing = next - point;
		const float     spread   = glm::length(incoming) * glm::length(outgoing) * glm::distance(previous, next);

		deepest = std::max(deepest, std::abs((*road)[index].y - RollingHills(point.x, point.y)));
		if (spread > 0.f)
			sharpest = std::max(sharpest, 2.f * std::abs(incoming.x * outgoing.y - incoming.y * outgoing.x) / spread);
	}

	EXPECT_LT(deepest, REFINED_CUT_LIMIT) << "the refined alignment should hug the ground instead of needing earthworks";
	EXPECT_GT(1.f / sharpest, road_class.minCurveRadius) << "the refined alignment should stay within the minimum curve radius";
}




TEST(RoadPlanner, TooSteepRoutesFollowTheGroundInsteadOfRamping)
{
	const trok::RoadClass                       road_class = TestRoadClass();
	const std::optional<std::vector<glm::vec3>> road       = trok::PlanRoad(START, END, road_class, SteepSlope);

	ASSERT_TRUE(road.has_value());

	const float buildable = road_class.maxFillHeight;
	float       deepest   = 0.f;
	float       steepest  = 0.f;

	for (size_t index = 0; index < road->size(); index++)
	{
		const glm::vec3& point = (*road)[index];
		deepest = std::max(deepest, std::abs(point.y - SteepSlope(point.x, point.z)));

		if (index > 0)
		{
			const glm::vec3& previous = (*road)[index - 1];
			const float      run      = glm::distance(glm::vec2(previous.x, previous.z), glm::vec2(point.x, point.z));
			steepest = std::max(steepest, std::abs(point.y - previous.y) / run);
		}
	}

	EXPECT_LE(deepest, buildable + TOLERANCE) << "the profile must stay within what the embankment can actually build";
	EXPECT_LT(steepest, SLOPE_GRADIENT * GRADE_TOLERANCE) << "the road should be no steeper than the ground it follows";
}

TEST(RoadNetwork, SegmentLookupMatchesABruteForceScan)
{
	std::vector<glm::vec3> centreline;
	for (float along = 0.f; along <= ROAD_LENGTH; along += LOOKUP_SPACING)
		centreline.emplace_back(along, ROAD_HEIGHT, std::sin(along * 0.01f) * LOOKUP_WANDER);

	const trok::RoadNetwork network(centreline, TestRoadClass());

	for (float origin = -LOOKUP_RECT_SIZE; origin < ROAD_LENGTH + LOOKUP_RECT_SIZE; origin += LOOKUP_RECT_SIZE)
	{
		const glm::vec2 minimum = { origin, -LOOKUP_WANDER };
		const glm::vec2 maximum = minimum + glm::vec2(LOOKUP_RECT_SIZE, LOOKUP_WANDER * 2.f);

		std::vector<uint32_t> expected;
		for (uint32_t segment = 0; segment + 1 < centreline.size(); segment++)
		{
			const glm::vec3 middle = (centreline[segment] + centreline[segment + 1]) * 0.5f;
			if (middle.x >= minimum.x && middle.x < maximum.x && middle.z >= minimum.y && middle.z < maximum.y)
				expected.push_back(segment);
		}

		EXPECT_EQ(network.segmentsWithin(minimum, maximum), expected) << "for the rect starting at x = " << origin;
	}
}





TEST(RoadNetwork, ZeroSpreadCutsStraightDown)
{
	const trok::RoadClass  road_class = { .name = "test:vertical", .embankmentSpread = 0.f };
	const trok::RoadNetwork network({ { 0.f, 0.f, 0.f }, { ROAD_LENGTH, 0.f, 0.f } }, road_class);

	const float half     = road_class.halfWidth();
	const float flat     = half + road_class.gradingMargin;
	const float centre   = network.gradedHeight(ROAD_LENGTH * 0.5, 0.0, ROAD_HEIGHT);
	const float edge     = network.gradedHeight(ROAD_LENGTH * 0.5, half, ROAD_HEIGHT);
	const float past     = network.gradedHeight(ROAD_LENGTH * 0.5, flat + 0.01, ROAD_HEIGHT);
	const float far_away = network.gradedHeight(ROAD_LENGTH * 0.5, FAR_AWAY, ROAD_HEIGHT);

	EXPECT_FLOAT_EQ(centre, 0.f);
	EXPECT_FLOAT_EQ(edge,   0.f);
	EXPECT_FLOAT_EQ(past,     ROAD_HEIGHT) << "a zero spread should leave the terrain untouched right past the flattened shoulder";
	EXPECT_FLOAT_EQ(far_away, ROAD_HEIGHT);
	EXPECT_FALSE(std::isnan(past));
}

TEST(RoadNetwork, GradedTerrainStaysBelowTheDeck)
{
	const trok::RoadClass   road_class = TestRoadClass();
	const trok::RoadNetwork network({ { 0.f, 0.f, 0.f }, { ROAD_LENGTH, 0.f, 0.f } }, road_class);

	const float half = road_class.halfWidth();
	const float deck = road_class.surfaceOffset;
	float       worst = 0.f;
	float       worst_cut = 0.f;

	for (const float cut : { 0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f })
	{
		for (float phase = 0.f; phase < VERTEX_SPACING; phase += 0.25f)
		{
			for (float sample = -half - VERTEX_SPACING * 4.f; sample < half + VERTEX_SPACING * 4.f; sample += VERTEX_SPACING)
			{
				const float near_z = sample + phase;
				const float near_h = network.gradedHeight(ROAD_LENGTH * 0.5, near_z, cut);
				const float far_h  = network.gradedHeight(ROAD_LENGTH * 0.5, near_z + VERTEX_SPACING, cut);

				for (float step = 0.f; step <= 1.f; step += 0.1f)
				{
					if (std::abs(glm::mix(near_z, near_z + VERTEX_SPACING, step)) > half)
						continue;

					const float intrusion = glm::mix(near_h, far_h, step) - deck;
					if (intrusion > worst) { worst = intrusion; worst_cut = cut; }
				}
			}
		}
	}

	printf("worst intrusion above the deck: %.3f m (at a %.2f m cut)\n", worst, worst_cut);
	EXPECT_LE(worst, 0.f) << "the heightmap rises through the deck";
}
