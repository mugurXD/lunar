#include "test_biomes.hpp"

#include <trok/world/region_plan.hpp>
#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
	constexpr uint32_t         GENERATOR_VERSION = 2;
	constexpr int32_t          WORLD_SEED        = 1337;
	constexpr trok::BiomeIndex WEST_BIOME        = TALL_BIOME;
	constexpr trok::BiomeIndex EAST_BIOME        = FLAT_BIOME;
	constexpr double           BORDER_OFFSET     = 0.5;
	constexpr double           SETTLEMENT_RADIUS = 450.0;

	const lunar::World::WorldSettings SETTINGS = {};
	const auto                        BIOMES   = std::make_shared<const trok::BiomeLibrary>(TestBiomes());
	const auto                        CLIMATE  = std::make_shared<const trok::ClimateSampler>(WORLD_SEED);

	trok::RegionPlanner Planner()
	{
		return trok::RegionPlanner(GENERATOR_VERSION, WORLD_SEED, BIOMES, CLIMATE);
	}

	double CellCentre(int32_t region, uint32_t cell)
	{
		const double cell_size = trok::BiomeCellSize(SETTINGS);
		return (region * static_cast<double>(trok::BiomeCellsPerSide(SETTINGS)) + cell + 0.5) * cell_size;
	}

	trok::RegionPlan PlanWithBiome(trok::BiomeIndex biome)
	{
		trok::RegionPlan plan = Planner().plan({}, SETTINGS);
		std::ranges::fill(plan.biomes, biome);
		return plan;
	}

	trok::RegionPlan PlanWithSettlement()
	{
		trok::RegionPlan plan = PlanWithBiome(EAST_BIOME);
		plan.settlements.push_back({ .id = 0, .type = "small_town", .center = { 1200.5, -300.25 }, .radius = SETTLEMENT_RADIUS });
		return plan;
	}

	trok::RegionPlan SavedWithPalette(std::vector<std::string> palette, trok::BiomeIndex first, trok::BiomeIndex rest)
	{
		trok::RegionPlan plan = PlanWithBiome(rest);
		plan.biomePalette = std::move(palette);
		plan.biomes.front() = first;
		return plan;
	}
}

TEST(RegionPlan, PlannerFillsTheBiomeGrid)
{
	const lunar::World::RegionCoord coord = { 4, -2 };
	const trok::RegionPlan          plan  = Planner().plan(coord, SETTINGS);

	EXPECT_EQ(plan.generatorVersion,  GENERATOR_VERSION);
	EXPECT_EQ(plan.biomeCellsPerSide, trok::BiomeCellsPerSide(SETTINGS));
	EXPECT_EQ(plan.biomePalette,      BIOMES->getNames());
	EXPECT_EQ(plan.biomes.size(),     static_cast<size_t>(plan.biomeCellsPerSide) * plan.biomeCellsPerSide);
	EXPECT_TRUE(plan.settlements.empty());

	for (uint32_t cell = 0; cell < plan.biomeCellsPerSide; cell++)
		EXPECT_EQ(plan.getBiome(cell, cell), BIOMES->select(CLIMATE->sample(CellCentre(coord.x, cell), CellCentre(coord.z, cell)), 0));
}

TEST(RegionPlan, PlanningIsDeterministic)
{
	const lunar::World::RegionCoord coord = { -3, 9 };

	EXPECT_EQ(Planner().plan(coord, SETTINGS), Planner().plan(coord, SETTINGS));
	EXPECT_NE(Planner().plan(coord, SETTINGS), Planner().plan({ coord.x, coord.z + 1 }, SETTINGS));
}

TEST(RegionPlan, RestoringMapsSavedNamesToTheCurrentLibrary)
{
	const trok::RegionPlan saved    = SavedWithPalette({ TALL.name, FLAT.name }, 1, 0);
	const trok::RegionPlan restored = Planner().restore(saved, {}, SETTINGS);

	EXPECT_EQ(restored.biomePalette,   BIOMES->getNames());
	EXPECT_EQ(restored.biomes.front(), FLAT_BIOME);
	EXPECT_EQ(restored.biomes.back(),  TALL_BIOME);
}

TEST(RegionPlan, RestoringReplacesUnknownBiomesWithTheDefault)
{
	const trok::RegionPlan saved    = SavedWithPalette({ std::string(UNKNOWN_NAME), TALL.name }, 0, 1);
	const trok::RegionPlan restored = Planner().restore(saved, {}, SETTINGS);

	EXPECT_EQ(restored.biomes.front(), BIOMES->getDefault());
	EXPECT_EQ(restored.biomes.back(),  TALL_BIOME);
}

TEST(RegionPlan, BiomesResolveAcrossRegionBorders)
{
	const trok::RegionContext context(SETTINGS, {
		{ { -1, 0 }, std::make_shared<const trok::RegionPlan>(PlanWithBiome(WEST_BIOME)) },
		{ { 0,  0 }, std::make_shared<const trok::RegionPlan>(PlanWithBiome(EAST_BIOME)) }
	});

	EXPECT_EQ(trok::BiomeAt(context, -BORDER_OFFSET, BORDER_OFFSET), WEST_BIOME);
	EXPECT_EQ(trok::BiomeAt(context, BORDER_OFFSET,  BORDER_OFFSET), EAST_BIOME);
	EXPECT_FALSE(trok::BiomeAt(context, BORDER_OFFSET, -BORDER_OFFSET).has_value());
}

TEST(RegionPlan, PlansRoundTripThroughJson)
{
	const trok::RegionPlan plan = PlanWithSettlement();

	EXPECT_EQ(Fs::DeserializeJson<trok::RegionPlan>(trok::RegionPlan::Serialize(plan)), plan);
}

TEST(RegionPlan, MalformedPlansAreRejected)
{
	nlohmann::json missing_field = trok::RegionPlan::Serialize(PlanWithSettlement());
	missing_field.erase("biomes");

	nlohmann::json missing_palette = trok::RegionPlan::Serialize(PlanWithSettlement());
	missing_palette.erase("biomePalette");

	nlohmann::json outside_palette = trok::RegionPlan::Serialize(PlanWithSettlement());
	outside_palette["biomes"][0] = outside_palette["biomePalette"].size();

	nlohmann::json wrong_biome_count = trok::RegionPlan::Serialize(PlanWithSettlement());
	wrong_biome_count["biomes"].erase(0);

	nlohmann::json broken_settlement = trok::RegionPlan::Serialize(PlanWithSettlement());
	broken_settlement["settlements"][0].erase("center");

	nlohmann::json future_format = trok::RegionPlan::Serialize(PlanWithSettlement());
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(missing_field).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(missing_palette).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(outside_palette).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(wrong_biome_count).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(broken_settlement).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(future_format).has_value());
}
