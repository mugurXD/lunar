#include "test_biomes.hpp"

#include <trok/world/region_plan.hpp>
#include <trok/world/shapers.hpp>
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
	constexpr double           MIN_SPACING_SHARE = 0.5;
	constexpr int32_t          SURVEY_RADIUS     = 3;
	constexpr double           FAR_FROM_RIVER    = 400.0;
	constexpr double           RIVER_CUT_LIMIT   = 31.0;
	constexpr float            COASTAL_SEA_LEVEL = 200.f;
	constexpr int              STRAIGHT_RIVER_POINTS  = 20;
	constexpr double           STRAIGHT_RIVER_SPACING = 125.0;
	constexpr double           STRAIGHT_RIVER_WIDTH   = 60.0;
	constexpr double           STRAIGHT_RIVER_FALL    = 2.0;

	const lunar::World::WorldSettings SETTINGS = {};
	const auto                        BIOMES   = std::make_shared<const trok::BiomeLibrary>(TestBiomes());
	const auto                        CLIMATE  = std::make_shared<const trok::ClimateSampler>(WORLD_SEED);
	const trok::ElevationCurve        TERRAIN  = { .points = { { -1.f, 0.f }, { 1.f, 600.f } }, .seaLevel = -100.f };

	trok::RegionPlanner Planner()
	{
		return trok::RegionPlanner(GENERATOR_VERSION, WORLD_SEED, BIOMES, CLIMATE, TERRAIN);
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
	EXPECT_LE(plan.settlements.size(), 1u);

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

TEST(RegionPlanner, PlacesAtMostOneSettlementInsideEachRegion)
{
	const trok::RegionPlanner planner     = Planner();
	const double              region_size = static_cast<double>(SETTINGS.regionChunks) * SETTINGS.getChunkSize();
	size_t                    placed      = 0;

	for (int32_t z = -SURVEY_RADIUS; z <= SURVEY_RADIUS; z++)
	{
		for (int32_t x = -SURVEY_RADIUS; x <= SURVEY_RADIUS; x++)
		{
			const trok::RegionPlan plan = planner.plan({ x, z }, SETTINGS);
			ASSERT_LE(plan.settlements.size(), 1u);

			if (plan.settlements.empty())
				continue;

			const trok::Settlement& settlement = plan.settlements.front();
			placed++;

			EXPECT_GE(settlement.center.x, x * region_size);
			EXPECT_LT(settlement.center.x, (x + 1) * region_size);
			EXPECT_GE(settlement.center.y, z * region_size);
			EXPECT_LT(settlement.center.y, (z + 1) * region_size);
			EXPECT_GT(settlement.radius, 0.0);
			EXPECT_FALSE(settlement.type.empty());
		}
	}

	EXPECT_GT(placed, 0u) << "habitable biomes should produce settlements somewhere";
}

TEST(RegionPlanner, SettlementsAreDeterministic)
{
	EXPECT_EQ(Planner().plan({ 2, -1 }, SETTINGS).settlements, Planner().plan({ 2, -1 }, SETTINGS).settlements);
}

TEST(RegionPlanner, UninhabitableBiomesGetNoSettlements)
{
	trok::Biome flat = FLAT;
	trok::Biome tall = TALL;

	flat.habitable = false;
	tall.habitable = false;

	const auto                library = std::make_shared<const trok::BiomeLibrary>(*trok::BiomeLibrary::Create({ flat, tall }, flat.name));
	const trok::RegionPlanner planner(GENERATOR_VERSION, WORLD_SEED, library, CLIMATE, TERRAIN);

	for (int32_t x = -SURVEY_RADIUS; x <= SURVEY_RADIUS; x++)
		EXPECT_TRUE(planner.plan({ x, 0 }, SETTINGS).settlements.empty());
}

TEST(RegionPlanner, SettlementsKeepTheirDistanceApart)
{
	const trok::RegionPlanner planner     = Planner();
	const double              region_size = static_cast<double>(SETTINGS.regionChunks) * SETTINGS.getChunkSize();

	std::vector<glm::dvec2> centres;
	for (int32_t z = -SURVEY_RADIUS; z <= SURVEY_RADIUS; z++)
		for (int32_t x = -SURVEY_RADIUS; x <= SURVEY_RADIUS; x++)
			for (const trok::Settlement& settlement : planner.plan({ x, z }, SETTINGS).settlements)
				centres.push_back(settlement.center);

	ASSERT_GT(centres.size(), 1u);

	for (size_t first = 0; first < centres.size(); first++)
		for (size_t second = first + 1; second < centres.size(); second++)
			EXPECT_GE(glm::distance(centres[first], centres[second]), region_size * MIN_SPACING_SHARE)
				<< "settlements " << first << " and " << second << " are too close together";
}



TEST(River, BedsOnlyEverDescend)
{
	const trok::River river = trok::TraceRiver(1, { 512.0, -300.0 }, TERRAIN, *CLIMATE, *BIOMES);

	ASSERT_GE(river.points.size(), 2u);

	for (size_t index = 1; index < river.points.size(); index++)
		EXPECT_LE(river.points[index].bed, river.points[index - 1].bed) << "the river climbs at point " << index;
}

trok::River StraightRiver()
{
	trok::River river = { .id = 1 };

	for (int step = 0; step <= STRAIGHT_RIVER_POINTS; step++)
		river.points.push_back({ .position = { step * STRAIGHT_RIVER_SPACING, 0.0 },
		                         .width    = STRAIGHT_RIVER_WIDTH,
		                         .bed      = -step * STRAIGHT_RIVER_FALL });

	river.minimum = { 0.0, 0.0 };
	river.maximum = { STRAIGHT_RIVER_POINTS * STRAIGHT_RIVER_SPACING, 0.0 };
	return river;
}

TEST(River, CarvingCutsAChannelInsideASmoothValley)
{
	const trok::River river   = StraightRiver();
	const double      middle  = STRAIGHT_RIVER_POINTS * STRAIGHT_RIVER_SPACING * 0.5;
	const float       terrain = 1000.f;

	const auto carved_at = [&](double offset) { return lunar::World::ApplyShapes(trok::RiverShapes(river), middle, offset, terrain); };

	EXPECT_NEAR(carved_at(0.0), static_cast<float>(-middle / STRAIGHT_RIVER_SPACING * STRAIGHT_RIVER_FALL), 1.f)
		<< "the channel should reach the river bed";
	EXPECT_FLOAT_EQ(carved_at(trok::RiverReach() * 2.0), terrain) << "terrain beyond the valley should be untouched";

	float previous = carved_at(0.0);
	for (double offset = STRAIGHT_RIVER_WIDTH; offset < trok::RiverValleyWidth(STRAIGHT_RIVER_WIDTH); offset += STRAIGHT_RIVER_WIDTH)
	{
		const float here = carved_at(offset);

		EXPECT_GE(here, previous) << "the valley should rise steadily away from the river at " << offset << " m";
		EXPECT_LE(here, terrain);
		previous = here;
	}
}

TEST(River, WaterFillsExactlyTheCarvedChannel)
{
	const trok::River river   = StraightRiver();
	const double      middle  = STRAIGHT_RIVER_POINTS * STRAIGHT_RIVER_SPACING * 0.5;
	const double      surface = -middle / STRAIGHT_RIVER_SPACING * STRAIGHT_RIVER_FALL + trok::RiverDepth(STRAIGHT_RIVER_WIDTH);
	const float       terrain = 1000.f;

	const auto carved_at = [&](double offset) { return static_cast<double>(lunar::World::ApplyShapes(trok::RiverShapes(river), middle, offset, terrain)); };

	EXPECT_LT(carved_at(STRAIGHT_RIVER_WIDTH * 0.25), surface) << "the middle of the river should be below the waterline";
	EXPECT_GT(carved_at(STRAIGHT_RIVER_WIDTH * 0.75), surface) << "the bank should be above the waterline, or water leaves a dry trench";
}

TEST(RegionPlanner, RiversAreDeterministic)
{
	EXPECT_EQ(Planner().plan({ 1, 1 }, SETTINGS).rivers, Planner().plan({ 1, 1 }, SETTINGS).rivers);
}





TEST(RegionPlanner, TownsAreNeverFoundedBelowSeaLevel)
{
	const trok::ElevationCurve coastal = { .points = { { -1.f, 0.f }, { 1.f, 600.f } }, .seaLevel = COASTAL_SEA_LEVEL };
	const trok::RegionPlanner  planner(GENERATOR_VERSION, WORLD_SEED, BIOMES, CLIMATE, coastal);

	int towns = 0;

	for (int32_t z = -SURVEY_RADIUS; z <= SURVEY_RADIUS; z++)
	{
		for (int32_t x = -SURVEY_RADIUS; x <= SURVEY_RADIUS; x++)
		{
			for (const trok::Settlement& settlement : planner.plan({ x, z }, SETTINGS).settlements)
			{
				towns++;
				EXPECT_GT(coastal.heightAt(CLIMATE->sampleContinentalness(settlement.center.x, settlement.center.y)), COASTAL_SEA_LEVEL)
					<< "a town was founded under water";
			}
		}
	}

	EXPECT_GT(towns, 0);
}


TEST(RiverShaper, DeclaresOnlyNearItsRivers)
{
	constexpr double FAR_FROM_ANY_RIVER = 4000.0;

	trok::RegionPlan plan = Planner().plan({ 0, 0 }, SETTINGS);
	plan.rivers = { StraightRiver() };

	const trok::RegionContext context(SETTINGS, { { { 0, 0 }, std::make_shared<const trok::RegionPlan>(std::move(plan)) } });
	const trok::RiverShaper   shaper;
	const glm::dvec2          middle = { STRAIGHT_RIVER_POINTS * STRAIGHT_RIVER_SPACING * 0.5, 0.0 };
	const glm::dvec2          away   = middle + glm::dvec2(0.0, FAR_FROM_ANY_RIVER);

	std::vector<lunar::World::ShapeDeclaration> near_river;
	shaper.declare(context, middle, middle, near_river);
	EXPECT_EQ(near_river.size(), 2u) << "a river should declare its valley and its channel";

	std::vector<lunar::World::ShapeDeclaration> far_away;
	shaper.declare(context, away, away, far_away);
	EXPECT_TRUE(far_away.empty());
}
