#include <trok/world/region_plan.hpp>
#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

namespace
{
	constexpr uint32_t      GENERATOR_VERSION = 2;
	constexpr trok::BiomeId WEST_BIOME        = 2;
	constexpr trok::BiomeId EAST_BIOME        = 1;
	constexpr double        BORDER_OFFSET     = 0.5;
	constexpr double        SETTLEMENT_RADIUS = 450.0;

	const lunar::World::WorldSettings SETTINGS = {};

	trok::RegionPlan PlanWithBiome(trok::BiomeId biome)
	{
		trok::RegionPlan plan = trok::RegionPlanner(GENERATOR_VERSION).plan({}, SETTINGS);
		std::ranges::fill(plan.biomes, biome);
		return plan;
	}

	trok::RegionPlan PlanWithSettlement()
	{
		trok::RegionPlan plan = PlanWithBiome(EAST_BIOME);
		plan.settlements.push_back({ .id = 0, .type = "small_town", .center = { 1200.5, -300.25 }, .radius = SETTLEMENT_RADIUS });
		return plan;
	}
}

TEST(RegionPlan, PlannerFillsTheBiomeGrid)
{
	const trok::RegionPlan plan = trok::RegionPlanner(GENERATOR_VERSION).plan({ 4, -2 }, SETTINGS);

	EXPECT_EQ(plan.generatorVersion,  GENERATOR_VERSION);
	EXPECT_EQ(plan.biomeCellsPerSide, trok::BiomeCellsPerSide(SETTINGS));
	EXPECT_EQ(plan.biomes.size(),     static_cast<size_t>(plan.biomeCellsPerSide) * plan.biomeCellsPerSide);
	EXPECT_TRUE(std::ranges::all_of(plan.biomes, [](trok::BiomeId biome) { return biome == trok::DEFAULT_BIOME; }));
	EXPECT_TRUE(plan.settlements.empty());
}

TEST(RegionPlan, BiomesResolveAcrossRegionBorders)
{
	const trok::RegionContext context(SETTINGS, {
		{ { -1, 0 }, std::make_shared<const trok::RegionPlan>(PlanWithBiome(WEST_BIOME)) },
		{ { 0,  0 }, std::make_shared<const trok::RegionPlan>(PlanWithBiome(EAST_BIOME)) }
	});

	EXPECT_EQ(trok::BiomeAt(context, -BORDER_OFFSET, BORDER_OFFSET), WEST_BIOME);
	EXPECT_EQ(trok::BiomeAt(context, BORDER_OFFSET,  BORDER_OFFSET), EAST_BIOME);
	EXPECT_EQ(trok::BiomeAt(context, BORDER_OFFSET, -BORDER_OFFSET), trok::DEFAULT_BIOME);
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

	nlohmann::json wrong_biome_count = trok::RegionPlan::Serialize(PlanWithSettlement());
	wrong_biome_count["biomes"].erase(0);

	nlohmann::json broken_settlement = trok::RegionPlan::Serialize(PlanWithSettlement());
	broken_settlement["settlements"][0].erase("center");

	nlohmann::json future_format = trok::RegionPlan::Serialize(PlanWithSettlement());
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(missing_field).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(wrong_biome_count).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(broken_settlement).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::RegionPlan>(future_format).has_value());
}
