#include "test_biomes.hpp"

#include <trok/world/dressers.hpp>
#include <trok/world/terrain_generator.hpp>
#include <trok/world/road.hpp>
#include <trok/world/road_service.hpp>
#include <trok/world/shapers.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/world/terrain.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
	constexpr int32_t WORLD_SEED       = 1337;
	constexpr int32_t OTHER_SEED       = 4242;
	constexpr float   HEIGHT_TOLERANCE = 0.5f;
	constexpr float   MAX_STEP_DELTA   = 8.f;
	constexpr double  CLIMATE_WALK     = 40000.0;
	constexpr double  CENTRE_CELL      = 4.0;
	constexpr float   RAISED_ELEVATION     = 100.f;
	constexpr size_t  VERTICES_PER_SEGMENT = trok::QUAD_CORNERS * 3;
	constexpr size_t  QUAD_TOP_LEFT        = 0;

	const lunar::World::WorldSettings SETTINGS  = { .sampleReachChunks = trok::BIOME_SAMPLE_REACH_CHUNKS };
	const auto                        BIOMES    = std::make_shared<const trok::BiomeLibrary>(TestBiomes());
	const auto                        ONLY_FLAT = std::make_shared<const trok::BiomeLibrary>(*trok::BiomeLibrary::Create({ FLAT }, FLAT.name));
	const auto                        ONLY_TALL = std::make_shared<const trok::BiomeLibrary>(*trok::BiomeLibrary::Create({ TALL }, TALL.name));
	const auto                        CLIMATE   = std::make_shared<const trok::ClimateSampler>(WORLD_SEED);
	const trok::ElevationCurve        FLAT_LAND = { .seaLevel = -1000.f };
	const trok::ElevationCurve        HIGHLAND  = { .points = { { -1.f, RAISED_ELEVATION }, { 1.f, RAISED_ELEVATION } }, .seaLevel = -1000.f };
	const glm::vec3                   UP        = { 0.f, 1.f, 0.f };
	const glm::vec3                   CLIFF     = { 1.f, 0.f, 0.f };

	trok::TerrainGenerator MakeGenerator(int32_t seed, const trok::ElevationCurve& elevation = FLAT_LAND, std::shared_ptr<const trok::BiomeLibrary> biomes = BIOMES)
	{
		return trok::TerrainGenerator(std::move(biomes), CLIMATE, elevation, seed);
	}

	std::vector<lunar::World::DressedMesh> Dress(const trok::TerrainDresser&   dresser,
	                                             const trok::TerrainGenerator& generator,
	                                             const trok::RegionContext&    context,
	                                             lunar::World::ChunkCoord      coord)
	{
		std::vector<lunar::World::DressedMesh> output;
		dresser.dress(context, coord, SETTINGS, [&](double x, double z) { return generator.sampleHeight(context, x, z); }, output);
		return output;
	}

	std::shared_ptr<const trok::RegionPlan> PlanOf(trok::BiomeIndex west, trok::BiomeIndex east)
	{
		const uint32_t cells_per_side = trok::BiomeCellsPerSide(SETTINGS);

		trok::RegionPlan plan =
		{
			.biomeCellsPerSide = cells_per_side,
			.biomePalette      = BIOMES->getNames(),
			.biomes            = std::vector<trok::BiomeIndex>(static_cast<size_t>(cells_per_side) * cells_per_side, west)
		};

		for (uint32_t cell_z = 0; cell_z < cells_per_side; cell_z++)
			for (uint32_t cell_x = cells_per_side / 2; cell_x < cells_per_side; cell_x++)
				plan.biomes[static_cast<size_t>(cell_z) * cells_per_side + cell_x] = east;

		return std::make_shared<const trok::RegionPlan>(std::move(plan));
	}

	trok::RegionContext ContextOf(trok::BiomeIndex west, trok::BiomeIndex east)
	{
		return trok::RegionContext(SETTINGS, { { { 0, 0 }, PlanOf(west, east) } });
	}

	double CellCentre(double cell)
	{
		return (cell + 0.5) * trok::BiomeCellSize(SETTINGS);
	}

	lunar::World::Heightmap ChunkHeights(const trok::TerrainGenerator& generator, const trok::RegionContext& context, lunar::World::ChunkCoord coord)
	{
		return lunar::World::SampleHeightmap([&](double x, double z) { return generator.sampleHeight(context, x, z); }, coord, SETTINGS);
	}
}

TEST(TrokTerrain, GenerationIsDeterministicPerSeed)
{
	const trok::RegionContext      context = ContextOf(FLAT_BIOME, TALL_BIOME);
	const lunar::World::ChunkCoord coord   = { 2, 7 };

	EXPECT_EQ(ChunkHeights(MakeGenerator(WORLD_SEED), context, coord), ChunkHeights(MakeGenerator(WORLD_SEED), context, coord));
	EXPECT_NE(ChunkHeights(MakeGenerator(WORLD_SEED), context, coord), ChunkHeights(MakeGenerator(OTHER_SEED), context, coord));
}

TEST(TrokTerrain, SingleBiomeWorldsStayWithinThatBiome)
{
	const trok::TerrainGenerator flat    = MakeGenerator(WORLD_SEED, FLAT_LAND, ONLY_FLAT);
	const trok::TerrainGenerator tall    = MakeGenerator(WORLD_SEED, FLAT_LAND, ONLY_TALL);
	const trok::RegionContext    context = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre  = CellCentre(CENTRE_CELL);

	EXPECT_NEAR(flat.sampleHeight(context, centre, centre), FLAT.terrain.heightOffset, FLAT.terrain.amplitude + HEIGHT_TOLERANCE);
	EXPECT_NEAR(tall.sampleHeight(context, centre, centre), TALL.terrain.heightOffset, TALL.terrain.amplitude + HEIGHT_TOLERANCE);
}

TEST(TrokTerrain, TerrainIsContinuousAcrossBiomes)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, FLAT_BIOME);

	float previous = generator.sampleHeight(context, 0.0, 0.0);
	float lowest   = previous;
	float highest  = previous;

	for (double x = SETTINGS.vertexSpacing; x <= CLIMATE_WALK; x += SETTINGS.vertexSpacing)
	{
		const float height = generator.sampleHeight(context, x, 0.0);
		ASSERT_LE(std::abs(height - previous), MAX_STEP_DELTA) << "a cliff at x = " << x;

		lowest   = std::min(lowest, height);
		highest  = std::max(highest, height);
		previous = height;
	}

	EXPECT_GT(highest - lowest, TALL.terrain.heightOffset * 0.5f) << "the walk should cross from flat into tall climate";
}

TEST(TrokTerrain, TerrainIgnoresTheBiomeCells)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
	const trok::RegionContext    flat      = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const trok::RegionContext    tall      = ContextOf(TALL_BIOME, TALL_BIOME);
	const double                 centre    = CellCentre(CENTRE_CELL);

	EXPECT_EQ(generator.sampleHeight(flat, centre, centre), generator.sampleHeight(tall, centre, centre)) << "terrain follows the climate, not the painted cells";
	EXPECT_EQ(generator.sampleColor(flat, centre, centre, 0.f, UP), generator.sampleColor(tall, centre, centre, 0.f, UP));
}

TEST(TrokTerrain, SteepSlopesAreRock)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED, FLAT_LAND, ONLY_FLAT);
	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre    = CellCentre(CENTRE_CELL);

	EXPECT_EQ(generator.sampleColor(context, centre, centre, 0.f, CLIFF), FLAT.colors.rockColor);
	EXPECT_NE(generator.sampleColor(context, centre, centre, 0.f, UP),    FLAT.colors.rockColor);
}

TEST(TrokTerrain, TheElevationCurveLiftsTheWholeTerrain)
{
	const trok::TerrainGenerator flat    = MakeGenerator(WORLD_SEED);
	const trok::TerrainGenerator raised  = MakeGenerator(WORLD_SEED, HIGHLAND);
	const trok::RegionContext    context = ContextOf(FLAT_BIOME, TALL_BIOME);
	const double                 centre  = CellCentre(CENTRE_CELL);

	EXPECT_FLOAT_EQ(raised.sampleHeight(context, centre, centre) - flat.sampleHeight(context, centre, centre), RAISED_ELEVATION);
}

TEST(TrokTerrain, ColorsFollowTheHeightAboveTheCurve)
{
	const trok::TerrainGenerator flat    = MakeGenerator(WORLD_SEED);
	const trok::TerrainGenerator raised  = MakeGenerator(WORLD_SEED, HIGHLAND);
	const trok::RegionContext    context = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre  = CellCentre(CENTRE_CELL);

	EXPECT_EQ(raised.sampleColor(context, centre, centre, RAISED_ELEVATION, UP), flat.sampleColor(context, centre, centre, 0.f, UP));
}

TEST(TrokTerrain, RoadsGradeTheTerrainAndAddAsphalt)
{
	constexpr float ROAD_HEIGHT   = -80.f;
	constexpr float FAR_FROM_ROAD = 400.f;

	const trok::RoadClass        road_class = {};
	const trok::RegionContext    context    = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre     = CellCentre(CENTRE_CELL);
	const auto                   roads      = std::make_shared<trok::RoadLayer>();
	trok::TerrainGenerator       generator  = MakeGenerator(WORLD_SEED);

	generator.addShaper(std::make_shared<const trok::RoadShaper>(roads));

	const float natural      = generator.sampleHeight(context, centre, centre);
	const float natural_away = generator.sampleHeight(context, centre, centre + FAR_FROM_ROAD);
	roads->set(std::make_shared<const trok::RoadNetwork>(
		std::vector<glm::vec3> { { centre - FAR_FROM_ROAD, ROAD_HEIGHT, centre }, { centre + FAR_FROM_ROAD, ROAD_HEIGHT, centre } },
		road_class));

	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre), ROAD_HEIGHT);
	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre + FAR_FROM_ROAD), natural_away);
	EXPECT_NE(natural, ROAD_HEIGHT);

	const std::vector<lunar::World::DressedMesh> dressing = Dress(trok::RoadDresser(roads), generator, context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS));
	ASSERT_EQ(dressing.size(), 1u);
	EXPECT_EQ(dressing.front().colliderCategory, lunar::Physics::ROAD_CATEGORY) << "asphalt must collide as road for the grip to apply";
	EXPECT_FALSE(dressing.front().translucent);

	const lunar::Render::MeshData& mesh = dressing.front().mesh;
	EXPECT_FALSE(mesh.vertices.empty());
	EXPECT_EQ(mesh.indices.size(), mesh.vertices.size() / trok::QUAD_CORNERS * 6);
	EXPECT_EQ(mesh.vertices.size() % VERTICES_PER_SEGMENT, 0u);

	const float surface = ROAD_HEIGHT + road_class.surfaceOffset;
	for (size_t vertex = 0; vertex < mesh.vertices.size(); vertex++)
	{
		EXPECT_LE(mesh.vertices[vertex].position.y, surface + 0.001f);

		if (vertex % VERTICES_PER_SEGMENT < trok::QUAD_CORNERS)
		{
			EXPECT_NEAR(mesh.vertices[vertex].position.y, surface, 0.001f);
			EXPECT_GT(mesh.vertices[vertex].normal.y, 0.9f);
		}
	}
}

TEST(TrokTerrain, LowFillDropsStraightDownToTheGround)
{
	constexpr float FILL_HEIGHT   = 2.f;
	constexpr float FAR_FROM_ROAD = 400.f;
	constexpr float POINT_SPACING = 8.f;

	const trok::RegionContext context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double              centre    = CellCentre(CENTRE_CELL);
	const auto                roads     = std::make_shared<trok::RoadLayer>();
	trok::TerrainGenerator    generator = MakeGenerator(WORLD_SEED);
	generator.addShaper(std::make_shared<const trok::RoadShaper>(roads));

	std::vector<glm::vec3> following;
	for (double along = -FAR_FROM_ROAD; along <= FAR_FROM_ROAD; along += POINT_SPACING)
		following.emplace_back(centre + along, generator.sampleHeight(context, centre + along, centre) + FILL_HEIGHT, centre);

	const float natural = generator.sampleHeight(context, centre, centre);
	roads->set(std::make_shared<const trok::RoadNetwork>(std::move(following), trok::RoadClass {}));

	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre), natural) << "roads never raise the terrain";

	const std::vector<lunar::World::DressedMesh> dressing = Dress(trok::RoadDresser(roads), generator, context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS));
	ASSERT_EQ(dressing.size(), 1u);

	const lunar::Render::MeshData& mesh = dressing.front().mesh;
	ASSERT_EQ(mesh.vertices.size() % VERTICES_PER_SEGMENT, 0u) << "low fill must not grow bridge geometry";

	const glm::vec3 top  = mesh.vertices[QUAD_TOP_LEFT].position;
	const glm::vec3 base = mesh.vertices[trok::QUAD_CORNERS].position;

	EXPECT_LT(base.y, top.y - 1.f) << "the side should reach down to the natural ground";
	EXPECT_NEAR(glm::distance(glm::vec2(base.x, base.z), glm::vec2(top.x, top.z)), 0.f, 0.001f) << "the side should drop straight down";
}

TEST(TrokTerrain, HighRoadsBecomeBridgesOnPillars)
{
	constexpr float BRIDGE_HEIGHT = 80.f;
	constexpr float FAR_FROM_ROAD = 400.f;

	const trok::RoadClass     road_class = {};
	const trok::RegionContext context    = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double              centre     = CellCentre(CENTRE_CELL);
	const auto                roads      = std::make_shared<trok::RoadLayer>();
	trok::TerrainGenerator    generator  = MakeGenerator(WORLD_SEED);
	generator.addShaper(std::make_shared<const trok::RoadShaper>(roads));

	const float natural = generator.sampleHeight(context, centre, centre);
	roads->set(std::make_shared<const trok::RoadNetwork>(
		std::vector<glm::vec3> { { centre - FAR_FROM_ROAD, natural + BRIDGE_HEIGHT, centre }, { centre + FAR_FROM_ROAD, natural + BRIDGE_HEIGHT, centre } },
		road_class));

	const std::vector<lunar::World::DressedMesh> dressing = Dress(trok::RoadDresser(roads), generator, context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS));
	ASSERT_EQ(dressing.size(), 1u);

	const lunar::Render::MeshData& mesh    = dressing.front().mesh;
	const float                    surface = natural + BRIDGE_HEIGHT + road_class.surfaceOffset;

	EXPECT_NEAR(mesh.vertices[trok::QUAD_CORNERS].position.y, surface - road_class.edgeDepth, 0.001f) << "a bridge deck should only be as thick as its edge";

	const auto lowest = std::ranges::min_element(mesh.vertices, {}, [](const lunar::Render::Vertex& vertex) { return vertex.position.y; });
	EXPECT_LT(lowest->position.y, natural) << "pillars should reach the ground";
}

TEST(TrokTerrain, CurvedRoadsFormAContinuousStrip)
{
	constexpr float ROAD_HEIGHT = -40.f;
	constexpr float ARC_RADIUS  = 120.f;
	constexpr int   ARC_POINTS  = 24;

	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre    = CellCentre(CENTRE_CELL);
	const auto                   roads     = std::make_shared<trok::RoadLayer>();
	trok::TerrainGenerator       generator = MakeGenerator(WORLD_SEED);

	std::vector<glm::vec3> arc;
	for (int index = 0; index < ARC_POINTS; index++)
	{
		const float angle = glm::radians(180.f) * static_cast<float>(index) / static_cast<float>(ARC_POINTS - 1);
		arc.emplace_back(centre + ARC_RADIUS * std::cos(angle), ROAD_HEIGHT, centre + ARC_RADIUS * std::sin(angle));
	}

	const glm::vec3 on_the_arc = arc[ARC_POINTS / 2];
	roads->set(std::make_shared<const trok::RoadNetwork>(std::move(arc), trok::RoadClass {}));

	const std::vector<lunar::World::DressedMesh> dressing = Dress(trok::RoadDresser(roads), generator, context, lunar::World::ChunkAt(on_the_arc, SETTINGS));
	ASSERT_EQ(dressing.size(), 1u);

	const lunar::Render::MeshData& mesh = dressing.front().mesh;
	ASSERT_GE(mesh.vertices.size(), VERTICES_PER_SEGMENT * 2);
	for (size_t segment = 0; segment + 1 < mesh.vertices.size() / VERTICES_PER_SEGMENT; segment++)
	{
		const size_t current = segment * VERTICES_PER_SEGMENT;
		const size_t next    = current + VERTICES_PER_SEGMENT;

		EXPECT_EQ(mesh.vertices[current + 2].position, mesh.vertices[next].position)     << "gap on the left side of segment " << segment;
		EXPECT_EQ(mesh.vertices[current + 3].position, mesh.vertices[next + 1].position) << "gap on the right side of segment " << segment;
	}
}

TEST(RoadService, PlansInTheBackgroundAndKeepsEveryRoad)
{
	constexpr size_t WORKERS         = 2;
	constexpr int    MAX_ROUNDS      = 100;
	constexpr float  SHORT_ROAD      = 150.f;
	constexpr float  ROAD_SEPARATION = 400.f;

	lunar::JobSystem          jobs(WORKERS);
	const auto                generator = std::make_shared<trok::TerrainGenerator>(ONLY_FLAT, CLIMATE, FLAT_LAND, WORLD_SEED);
	trok::RoadService         roads(jobs, generator, trok::RoadClass {}, {});
	const trok::RegionContext context = ContextOf(FLAT_BIOME, FLAT_BIOME);

	const auto finish = [&] {
		for (int round = 0; round < MAX_ROUNDS; round++)
		{
			if (roads.update())
				return true;

			jobs.waitIdle();
			jobs.processCompleted();
		}

		return false;
	};

	const trok::RoadLink first  = { { 0.f, 0.f }, { SHORT_ROAD, 0.f } };
	const trok::RoadLink second = { { 0.f, ROAD_SEPARATION }, { SHORT_ROAD, ROAD_SEPARATION } };

	roads.plan(std::span(&first, 1), context);
	EXPECT_EQ(roads.getNetwork(), nullptr) << "nothing is published until the planning jobs finish";
	ASSERT_TRUE(finish());
	ASSERT_NE(roads.getNetwork(), nullptr);
	EXPECT_EQ(roads.getNetwork()->getRoadCount(), 1u);

	roads.plan(std::span(&second, 1), context);
	ASSERT_TRUE(finish());
	EXPECT_EQ(roads.getNetwork()->getRoadCount(), 2u) << "a new road is added to the network instead of replacing it";
}

TEST(TrokTerrain, WithoutARoadTheRoadDresserAddsNothing)
{
	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                 centre    = CellCentre(CENTRE_CELL);
	const auto                   roads     = std::make_shared<trok::RoadLayer>();
	trok::TerrainGenerator       generator = MakeGenerator(WORLD_SEED);

	EXPECT_TRUE(Dress(trok::RoadDresser(roads), generator, context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS)).empty());
}

TEST(TrokTerrain, WaterSurfaceSitsOnTheRiverAndIsTranslucent)
{
	constexpr double RIVER_BED     = -5.0;
	constexpr double RIVER_WIDTH   = 12.0;
	constexpr double RIVER_SPACING = 20.0;
	constexpr int    RIVER_SPAN    = 4;

	const double centre = CellCentre(CENTRE_CELL);

	trok::River river = { .id = 1 };
	for (int step = -RIVER_SPAN; step <= RIVER_SPAN; step++)
		river.points.push_back({ .position = { centre + step * RIVER_SPACING, centre }, .width = RIVER_WIDTH, .bed = RIVER_BED });

	river.minimum = river.points.front().position;
	river.maximum = river.points.back().position;

	trok::RegionPlan plan = *PlanOf(FLAT_BIOME, FLAT_BIOME);
	plan.rivers.push_back(river);

	const trok::RegionContext    context(SETTINGS, { { { 0, 0 }, std::make_shared<const trok::RegionPlan>(std::move(plan)) } });
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);

	const std::vector<lunar::World::DressedMesh> dressing = Dress(trok::RiverWaterDresser(), generator, context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS));
	ASSERT_EQ(dressing.size(), 1u);
	EXPECT_TRUE(dressing.front().translucent);
	EXPECT_EQ(dressing.front().colliderCategory, 0u) << "water must not become a collider";

	const lunar::Render::MeshData& mesh = dressing.front().mesh;
	ASSERT_FALSE(mesh.vertices.empty());

	const float level = static_cast<float>(RIVER_BED + trok::RiverDepth(RIVER_WIDTH));
	for (const lunar::Render::Vertex& vertex : mesh.vertices)
	{
		EXPECT_NEAR(vertex.position.y, level, 0.001f) << "the surface should sit one depth above the bed";
		EXPECT_LT(vertex.color.a, 1.f) << "water should be translucent";
	}
}

TEST(TrokTerrain, SeaCoversOnlySubmergedChunks)
{
	constexpr float HIGH_TIDE = 1000.f;
	constexpr float LOW_TIDE  = -1000.f;

	const trok::RegionContext      context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double                   centre    = CellCentre(CENTRE_CELL);
	const trok::TerrainGenerator   generator = MakeGenerator(WORLD_SEED);
	const lunar::World::ChunkCoord chunk     = lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS);

	EXPECT_TRUE(Dress(trok::SeaDresser(LOW_TIDE), generator, context, chunk).empty()) << "dry land should get no sea";

	const std::vector<lunar::World::DressedMesh> flooded = Dress(trok::SeaDresser(HIGH_TIDE), generator, context, chunk);
	ASSERT_EQ(flooded.size(), 1u);
	EXPECT_TRUE(flooded.front().translucent);
	EXPECT_EQ(flooded.front().colliderCategory, 0u);

	for (const lunar::Render::Vertex& vertex : flooded.front().mesh.vertices)
		EXPECT_FLOAT_EQ(vertex.position.y, HIGH_TIDE);
}
