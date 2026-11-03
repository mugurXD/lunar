#include "test_biomes.hpp"

#include <trok/world/decorators.hpp>
#include <trok/world/road.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/world/terrain.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string_view>
#include <vector>

namespace
{
	using lunar::World::DecorationInstance;

	constexpr int32_t WORLD_SEED      = 1337;
	constexpr int32_t OTHER_SEED      = 4242;
	constexpr float   GROUND_HEIGHT   = 10.f;
	constexpr float   SEA_LEVEL       = 0.f;
	constexpr float   UNDERWATER      = -5.f;
	constexpr float   STEEPNESS       = 2.f;
	constexpr float   CELL_SPACING    = 8.f;
	constexpr size_t  CELLS_PER_CHUNK = 64;
	constexpr float   HALF_DENSITY    = 0.5f;
	constexpr float   WIDE_FOOTPRINT  = 6.f;
	constexpr float   ROAD_CLEARANCE  = 3.f;
	constexpr double  ROAD_OVERHANG   = 100.0;
	constexpr double  TOWN_RADIUS     = 20.0;
	constexpr double  RIVER_WIDTH     = 6.0;
	constexpr double  RIVER_OFFSET    = 8.0;
	constexpr double  HALF            = 0.5;
	constexpr float   CLUSTER_FREQ    = 0.01f;
	constexpr float   RARE_CLUSTERS   = 1.f;

	const lunar::World::WorldSettings SETTINGS      = {};
	const lunar::World::ChunkCoord    CHUNK         = { 5, 5 };
	const lunar::World::ChunkCoord    NEIGHBOUR     = { 6, 5 };
	const trok::RegionContext         EMPTY_CONTEXT = trok::RegionContext(SETTINGS, {});
	const auto                        CLIMATE       = std::make_shared<const trok::ClimateSampler>(WORLD_SEED);
	const trok::DecorationRule        EVERY_CELL    = { .decoration = "test:tree", .spacing = CELL_SPACING, .density = 1.f, .clearance = 0.f };

	const auto CATALOG = std::make_shared<const lunar::World::DecorationCatalog>(lunar::World::DecorationCatalog {
		.types =
		{
			{ .name = "test:tree", .lods = { { "tree.glb", 100.f } } },
			{ .name = "test:rock", .lods = { { "rock.glb", 100.f } } }
		}
	});

	std::filesystem::path DataFile(std::string_view name)
	{
		return std::filesystem::path(TROK_DATA_DIRECTORY) / name;
	}

	trok::BiomeDecorator MakeDecorator(std::vector<trok::DecorationRule> rules, int32_t seed = WORLD_SEED, std::shared_ptr<const trok::RoadLayer> roads = nullptr)
	{
		trok::Biome biome = FLAT;
		biome.decorations = std::move(rules);

		return trok::BiomeDecorator(std::make_shared<const trok::BiomeLibrary>(*trok::BiomeLibrary::Create({ biome }, biome.name)), CLIMATE, CATALOG, std::move(roads), seed, SEA_LEVEL);
	}

	template<typename HeightFunction>
	std::vector<DecorationInstance> Decorate(const trok::BiomeDecorator& decorator, lunar::World::ChunkCoord coord, HeightFunction&& height_at, const trok::RegionContext& context = EMPTY_CONTEXT)
	{
		const lunar::World::Heightmap   heightmap = lunar::World::SampleHeightmap(height_at, coord, SETTINGS);
		std::vector<DecorationInstance> placed;
		decorator.decorate(context, coord, SETTINGS, heightmap, placed);
		return placed;
	}

	std::vector<DecorationInstance> DecorateFlat(const trok::BiomeDecorator& decorator, lunar::World::ChunkCoord coord = CHUNK, const trok::RegionContext& context = EMPTY_CONTEXT)
	{
		return Decorate(decorator, coord, [](double, double) { return GROUND_HEIGHT; }, context);
	}

	glm::dvec2 ChunkCorner(lunar::World::ChunkCoord coord)
	{
		return { lunar::World::SampleCoordinate(coord.x, 0, SETTINGS), lunar::World::SampleCoordinate(coord.z, 0, SETTINGS) };
	}

	glm::dvec2 AbsoluteOf(const DecorationInstance& instance, lunar::World::ChunkCoord coord = CHUNK)
	{
		return ChunkCorner(coord) + glm::dvec2(instance.position.x, instance.position.z);
	}

	std::optional<trok::Biome> RoundTrip(const trok::Biome& biome)
	{
		return Fs::DeserializeJson<trok::Biome>(trok::Biome::Serialize(biome));
	}
}

TEST(BiomeDecorator, PlacementIsDeterministicPerSeed)
{
	const std::vector<trok::DecorationRule> rules = { { .decoration = "test:tree", .spacing = CELL_SPACING, .density = HALF_DENSITY } };

	EXPECT_FALSE(DecorateFlat(MakeDecorator(rules)).empty());
	EXPECT_EQ(DecorateFlat(MakeDecorator(rules)), DecorateFlat(MakeDecorator(rules)));
	EXPECT_NE(DecorateFlat(MakeDecorator(rules)), DecorateFlat(MakeDecorator(rules, OTHER_SEED)));
}

TEST(BiomeDecorator, EveryCellHoldsOneDecorationInsideItsChunk)
{
	const std::vector<DecorationInstance> placed = DecorateFlat(MakeDecorator({ EVERY_CELL }));
	const float                           size   = SETTINGS.getChunkSize();
	ASSERT_EQ(placed.size(), CELLS_PER_CHUNK);

	for (const DecorationInstance& instance : placed)
	{
		EXPECT_GE(instance.position.x, 0.f);
		EXPECT_LT(instance.position.x, size);
		EXPECT_GE(instance.position.z, 0.f);
		EXPECT_LT(instance.position.z, size);
		EXPECT_FLOAT_EQ(instance.position.y, GROUND_HEIGHT - SETTINGS.maxFacetError) << "bases sink below the faceted surface";
	}
}

TEST(BiomeDecorator, IdsAreUniqueAcrossNeighbouringChunks)
{
	const trok::BiomeDecorator           decorator = MakeDecorator({ EVERY_CELL });
	std::set<lunar::World::DecorationId> ids;

	for (const lunar::World::ChunkCoord coord : { CHUNK, NEIGHBOUR })
		for (const DecorationInstance& instance : DecorateFlat(decorator, coord))
			EXPECT_TRUE(ids.insert(instance.id).second);
}

TEST(BiomeDecorator, NothingGrowsUnderTheSeaOrOnSteepGround)
{
	const trok::BiomeDecorator decorator = MakeDecorator({ EVERY_CELL });

	EXPECT_TRUE(Decorate(decorator, CHUNK, [](double, double) { return UNDERWATER; }).empty());
	EXPECT_TRUE(Decorate(decorator, CHUNK, [](double x, double) { return static_cast<float>(x) * STEEPNESS; }).empty());
}

TEST(BiomeDecorator, RoadsAreKeptClear)
{
	const glm::dvec2 corner   = ChunkCorner(CHUNK);
	const double     size     = SETTINGS.getChunkSize();
	const double     centre_z = corner.y + size * HALF;
	const auto       layer    = std::make_shared<trok::RoadLayer>();
	layer->set(std::make_shared<const trok::RoadNetwork>(std::vector<glm::vec3> {
		{ static_cast<float>(corner.x - ROAD_OVERHANG),        GROUND_HEIGHT, static_cast<float>(centre_z) },
		{ static_cast<float>(corner.x + size + ROAD_OVERHANG), GROUND_HEIGHT, static_cast<float>(centre_z) }
	}, trok::RoadClass { .name = "test:road" }));

	trok::DecorationRule rule = EVERY_CELL;
	rule.clearance = ROAD_CLEARANCE;

	const trok::RoadClass                 road_class = layer->get()->getRoadClass();
	const std::vector<DecorationInstance> placed     = DecorateFlat(MakeDecorator({ rule }, WORLD_SEED, layer));
	ASSERT_FALSE(placed.empty());
	EXPECT_LT(placed.size(), CELLS_PER_CHUNK);

	for (const DecorationInstance& instance : placed)
		EXPECT_GE(std::abs(AbsoluteOf(instance).y - centre_z), road_class.halfWidth() + road_class.maxEdgeRun + ROAD_CLEARANCE);
}

TEST(BiomeDecorator, TownsAndRiversAreKeptClear)
{
	const glm::dvec2 corner  = ChunkCorner(CHUNK);
	const double     size    = SETTINGS.getChunkSize();
	const glm::dvec2 centre  = corner + glm::dvec2(size * HALF);
	const double     river_z = corner.y + RIVER_OFFSET;

	trok::RegionPlan plan;
	plan.settlements.push_back({ .id = 1, .type = "test:town", .center = centre, .radius = TOWN_RADIUS });
	plan.rivers.push_back({ .id = 1, .minimum = { corner.x, river_z }, .maximum = { corner.x + size, river_z }, .points = {
		{ .position = { corner.x - size, river_z }, .width = RIVER_WIDTH },
		{ .position = { corner.x + size * 2.0, river_z }, .width = RIVER_WIDTH }
	} });

	const trok::RegionContext             context(SETTINGS, { { lunar::World::RegionAt(CHUNK, SETTINGS), std::make_shared<const trok::RegionPlan>(plan) } });
	const std::vector<DecorationInstance> placed = DecorateFlat(MakeDecorator({ EVERY_CELL }), CHUNK, context);
	ASSERT_FALSE(placed.empty());
	EXPECT_LT(placed.size(), CELLS_PER_CHUNK);

	for (const DecorationInstance& instance : placed)
	{
		EXPECT_GE(glm::distance(AbsoluteOf(instance), centre), TOWN_RADIUS);
		EXPECT_GE(std::abs(AbsoluteOf(instance).y - river_z), RIVER_WIDTH * HALF);
	}
}

TEST(BiomeDecorator, FootprintsKeepDecorationsApart)
{
	trok::DecorationRule trees = EVERY_CELL;
	trees.footprint = WIDE_FOOTPRINT;

	trok::DecorationRule rocks = trees;
	rocks.decoration = "test:rock";

	const std::vector<DecorationInstance> placed = DecorateFlat(MakeDecorator({ trees, rocks }));
	ASSERT_FALSE(placed.empty());

	for (size_t first = 0; first < placed.size(); first++)
		for (size_t second = first + 1; second < placed.size(); second++)
			EXPECT_GE(glm::distance(AbsoluteOf(placed[first]), AbsoluteOf(placed[second])), WIDE_FOOTPRINT * 2.f);
}

TEST(BiomeDecorator, DensityAndClusteringThinPlacementOut)
{
	trok::DecorationRule barren = EVERY_CELL;
	barren.density = 0.f;

	trok::DecorationRule clustered = EVERY_CELL;
	clustered.clusterFrequency = CLUSTER_FREQ;
	clustered.clusterThreshold = RARE_CLUSTERS;

	EXPECT_TRUE(DecorateFlat(MakeDecorator({ barren })).empty());
	EXPECT_LT(DecorateFlat(MakeDecorator({ clustered })).size(), CELLS_PER_CHUNK);
}

TEST(BiomeDecorator, UnknownDecorationsArePlacedNowhere)
{
	trok::DecorationRule unknown = EVERY_CELL;
	unknown.decoration = "test:unknown";

	EXPECT_TRUE(DecorateFlat(MakeDecorator({ unknown })).empty());
}

TEST(DecorationRules, RoundTripThroughBiomeJson)
{
	trok::Biome biome = FLAT;
	biome.decorations = { EVERY_CELL, { .decoration = "test:rock", .spacing = CELL_SPACING, .density = HALF_DENSITY, .maxAltitude = GROUND_HEIGHT, .tilt = HALF_DENSITY, .footprint = WIDE_FOOTPRINT } };

	EXPECT_EQ(RoundTrip(biome), biome);
	EXPECT_EQ(RoundTrip(FLAT), FLAT) << "biomes without rules still load";
}

TEST(DecorationRules, InvalidRulesAreRejected)
{
	const std::vector<trok::DecorationRule> invalid =
	{
		{ .decoration = "" },
		{ .decoration = "test:tree", .spacing = 0.f },
		{ .decoration = "test:tree", .density = 2.f },
		{ .decoration = "test:tree", .minScale = 2.f, .maxScale = 1.f },
		{ .decoration = "test:tree", .tilt = 2.f },
		{ .decoration = "test:tree", .minAltitude = GROUND_HEIGHT, .maxAltitude = SEA_LEVEL }
	};

	for (const trok::DecorationRule& rule : invalid)
	{
		trok::Biome biome = FLAT;
		biome.decorations = { rule };
		EXPECT_FALSE(RoundTrip(biome).has_value());
	}
}

TEST(DecorationRules, ShippedRulesUseShippedDecorations)
{
	const std::optional<trok::BiomeLibrary>             biomes  = Fs::LoadJson<trok::BiomeLibrary>(DataFile("biomes.json"));
	const std::optional<lunar::World::DecorationCatalog> catalog = Fs::LoadJson<lunar::World::DecorationCatalog>(DataFile("decorations.json"));
	ASSERT_TRUE(biomes.has_value());
	ASSERT_TRUE(catalog.has_value());

	for (const trok::Biome& biome : biomes->getBiomes())
		for (const trok::DecorationRule& rule : biome.decorations)
			EXPECT_TRUE(catalog->find(rule.decoration).has_value()) << biome.name << " uses " << rule.decoration;

	for (const lunar::World::DecorationType& type : catalog->types)
		for (const lunar::World::DecorationLod& lod : type.lods)
			EXPECT_TRUE(std::filesystem::exists(DataFile(lod.model))) << lod.model;
}
