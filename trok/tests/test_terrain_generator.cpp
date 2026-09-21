#include "test_biomes.hpp"

#include <trok/world/terrain_generator.hpp>
#include <trok/world/road.hpp>
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
	constexpr double  BLEND_DISTANCE   = 512.0;
	constexpr double  CENTRE_CELL      = 4.0;
	constexpr float   RAISED_ELEVATION     = 100.f;
	constexpr size_t  VERTICES_PER_SEGMENT = trok::QUAD_CORNERS * 3;

	const lunar::World::WorldSettings SETTINGS  = { .sampleReachChunks = trok::BIOME_SAMPLE_REACH_CHUNKS };
	const auto                        BIOMES    = std::make_shared<const trok::BiomeLibrary>(TestBiomes());
	const auto                        CLIMATE   = std::make_shared<const trok::ClimateSampler>(WORLD_SEED);
	const trok::ElevationCurve        FLAT_LAND = {};
	const trok::ElevationCurve        HIGHLAND  = { .points = { { -1.f, RAISED_ELEVATION }, { 1.f, RAISED_ELEVATION } } };
	const glm::vec3                   UP        = { 0.f, 1.f, 0.f };
	const glm::vec3                   CLIFF     = { 1.f, 0.f, 0.f };

	trok::TerrainGenerator MakeGenerator(int32_t seed, const trok::ElevationCurve& elevation = FLAT_LAND)
	{
		return trok::TerrainGenerator(BIOMES, CLIMATE, elevation, seed);
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

	void ExpectContinuousAcross(const trok::TerrainGenerator& generator, const trok::RegionContext& context, double border_x)
	{
		float previous = generator.sampleHeight(context, border_x - BLEND_DISTANCE, CellCentre(CENTRE_CELL));

		for (double x = border_x - BLEND_DISTANCE; x <= border_x + BLEND_DISTANCE; x += SETTINGS.vertexSpacing)
		{
			const float height = generator.sampleHeight(context, x, CellCentre(CENTRE_CELL));
			EXPECT_LE(std::abs(height - previous), MAX_STEP_DELTA) << "at x = " << x;
			previous = height;
		}
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

TEST(TrokTerrain, HeightsStayWithinTheBiomeOfTheirCell)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const trok::RegionContext    tall      = ContextOf(TALL_BIOME, TALL_BIOME);
	const double                 centre    = CellCentre(CENTRE_CELL);

	const float flat_height = generator.sampleHeight(context, centre, centre);
	const float tall_height = generator.sampleHeight(tall, centre, centre);

	EXPECT_NEAR(flat_height, FLAT.terrain.heightOffset, FLAT.terrain.amplitude + HEIGHT_TOLERANCE);
	EXPECT_NEAR(tall_height, TALL.terrain.heightOffset, TALL.terrain.amplitude + HEIGHT_TOLERANCE);
}

TEST(TrokTerrain, NeighbouringBiomesBlendContinuously)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
	const trok::RegionContext    context   = ContextOf(FLAT_BIOME, TALL_BIOME);
	const double                 border_x  = CellCentre(trok::BiomeCellsPerSide(SETTINGS) / 2.0 - 0.5);

	ExpectContinuousAcross(generator, context, border_x);

	EXPECT_GT(std::abs(generator.sampleHeight(context, border_x + BLEND_DISTANCE, CellCentre(CENTRE_CELL))
	                 - generator.sampleHeight(context, border_x - BLEND_DISTANCE, CellCentre(CENTRE_CELL))),
	          TALL.terrain.heightOffset * 0.5f);
}

TEST(TrokTerrain, BiomesBlendAcrossRegionBorders)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
	const trok::RegionContext    context(SETTINGS, {
		{ { -1, 0 }, PlanOf(TALL_BIOME, TALL_BIOME) },
		{ {  0, 0 }, PlanOf(FLAT_BIOME, FLAT_BIOME) }
	});

	ExpectContinuousAcross(generator, context, 0.0);
}

TEST(TrokTerrain, SteepSlopesAreRock)
{
	const trok::TerrainGenerator generator = MakeGenerator(WORLD_SEED);
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

	const trok::RoadClass     road_class = {};
	const trok::RegionContext context    = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double              centre     = CellCentre(CENTRE_CELL);
	trok::TerrainGenerator    generator  = MakeGenerator(WORLD_SEED);

	const float natural      = generator.sampleHeight(context, centre, centre);
	const float natural_away = generator.sampleHeight(context, centre, centre + FAR_FROM_ROAD);
	generator.setRoads(std::make_shared<const trok::RoadNetwork>(
		std::vector<glm::vec3> { { centre - FAR_FROM_ROAD, ROAD_HEIGHT, centre }, { centre + FAR_FROM_ROAD, ROAD_HEIGHT, centre } },
		road_class));

	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre), ROAD_HEIGHT);
	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre + FAR_FROM_ROAD), natural_away);
	EXPECT_NE(natural, ROAD_HEIGHT);

	lunar::Render::MeshData mesh;
	generator.buildDecorations(context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS), SETTINGS, mesh);

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

TEST(TrokTerrain, FillIsLeftToTheRoadGeometry)
{
	constexpr float ROAD_HEIGHT   = 80.f;
	constexpr float FAR_FROM_ROAD = 400.f;

	const trok::RegionContext context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double              centre    = CellCentre(CENTRE_CELL);
	trok::TerrainGenerator    generator = MakeGenerator(WORLD_SEED);

	const float natural = generator.sampleHeight(context, centre, centre);
	generator.setRoads(std::make_shared<const trok::RoadNetwork>(
		std::vector<glm::vec3> { { centre - FAR_FROM_ROAD, ROAD_HEIGHT, centre }, { centre + FAR_FROM_ROAD, ROAD_HEIGHT, centre } },
		trok::RoadClass {}));

	EXPECT_FLOAT_EQ(generator.sampleHeight(context, centre, centre), natural);

	lunar::Render::MeshData mesh;
	generator.buildDecorations(context, lunar::World::ChunkAt(glm::vec3(centre, 0.f, centre), SETTINGS), SETTINGS, mesh);

	ASSERT_GE(mesh.vertices.size(), VERTICES_PER_SEGMENT);

	const glm::vec3 top  = mesh.vertices[0].position;
	const glm::vec3 base = mesh.vertices[trok::QUAD_CORNERS].position;

	EXPECT_LT(base.y, top.y - 1.f) << "the embankment should reach down towards the natural ground";
	EXPECT_GT(glm::distance(glm::vec2(base.x, base.z), glm::vec2(top.x, top.z)), 1.f) << "the embankment should flare outwards";
}

TEST(TrokTerrain, CurvedRoadsFormAContinuousStrip)
{
	constexpr float ROAD_HEIGHT = 40.f;
	constexpr float ARC_RADIUS  = 120.f;
	constexpr int   ARC_POINTS  = 24;

	const trok::RegionContext context   = ContextOf(FLAT_BIOME, FLAT_BIOME);
	const double              centre    = CellCentre(CENTRE_CELL);
	trok::TerrainGenerator    generator = MakeGenerator(WORLD_SEED);

	std::vector<glm::vec3> arc;
	for (int index = 0; index < ARC_POINTS; index++)
	{
		const float angle = glm::radians(180.f) * static_cast<float>(index) / static_cast<float>(ARC_POINTS - 1);
		arc.emplace_back(centre + ARC_RADIUS * std::cos(angle), ROAD_HEIGHT, centre + ARC_RADIUS * std::sin(angle));
	}

	const glm::vec3 on_the_arc = arc[ARC_POINTS / 2];
	generator.setRoads(std::make_shared<const trok::RoadNetwork>(std::move(arc), trok::RoadClass {}));

	lunar::Render::MeshData mesh;
	generator.buildDecorations(context, lunar::World::ChunkAt(on_the_arc, SETTINGS), SETTINGS, mesh);

	ASSERT_GE(mesh.vertices.size(), VERTICES_PER_SEGMENT * 2);
	for (size_t segment = 0; segment + 1 < mesh.vertices.size() / VERTICES_PER_SEGMENT; segment++)
	{
		const size_t current = segment * VERTICES_PER_SEGMENT;
		const size_t next    = current + VERTICES_PER_SEGMENT;

		EXPECT_EQ(mesh.vertices[current + 2].position, mesh.vertices[next].position)         << "gap on the left side of segment " << segment;
		EXPECT_EQ(mesh.vertices[current + 3].position, mesh.vertices[next + 1].position)     << "gap on the right side of segment " << segment;
	}
}

