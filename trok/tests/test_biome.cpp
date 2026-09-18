#include "test_biomes.hpp"

#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

#include <set>

namespace
{
	constexpr uint64_t TIE_BREAKER       = 12345;
	constexpr uint64_t TIE_BREAKER_COUNT = 64;

	const trok::Climate INSIDE_FLAT  = { .temperature = 0.5f,  .moisture = -0.9f, .continentalness = -0.5f };
	const trok::Climate INSIDE_TALL  = { .temperature = -0.9f, .moisture = 0.9f,  .continentalness = 0.8f };
	const trok::Climate NEARER_FLAT  = { .temperature = 0.5f,  .moisture = 0.f,   .continentalness = 0.2f };
	const trok::Climate NEARER_TALL  = { .temperature = 0.5f,  .moisture = 0.f,   .continentalness = 0.3f };
	const trok::Climate COLD_LOWLAND = { .temperature = -0.5f, .moisture = 0.f,   .continentalness = -0.5f };

	trok::BiomeLibrary OverlappingBiomes()
	{
		trok::Biome overlapping = TALL;
		overlapping.climate     = FLAT.climate;

		return *trok::BiomeLibrary::Create({ FLAT, overlapping }, FLAT.name);
	}
}

TEST(Biomes, LibrariesRoundTripThroughJson)
{
	const trok::BiomeLibrary library = TestBiomes();

	EXPECT_EQ(Fs::DeserializeJson<trok::BiomeLibrary>(trok::BiomeLibrary::Serialize(library)), library);
}

TEST(Biomes, MalformedLibrariesAreRejected)
{
	nlohmann::json empty_library = trok::BiomeLibrary::Serialize(TestBiomes());
	empty_library["biomes"].clear();

	nlohmann::json duplicate_names = trok::BiomeLibrary::Serialize(TestBiomes());
	duplicate_names["biomes"][1]["name"] = duplicate_names["biomes"][0]["name"];

	nlohmann::json empty_name = trok::BiomeLibrary::Serialize(TestBiomes());
	empty_name["biomes"][1]["name"] = "";

	nlohmann::json unknown_default = trok::BiomeLibrary::Serialize(TestBiomes());
	unknown_default["default"] = UNKNOWN_NAME;

	nlohmann::json missing_default = trok::BiomeLibrary::Serialize(TestBiomes());
	missing_default.erase("default");

	nlohmann::json missing_terrain = trok::BiomeLibrary::Serialize(TestBiomes());
	missing_terrain["biomes"][0].erase("terrain");

	nlohmann::json short_color = trok::BiomeLibrary::Serialize(TestBiomes());
	short_color["biomes"][0]["colors"]["low"].erase(2);

	nlohmann::json future_format = trok::BiomeLibrary::Serialize(TestBiomes());
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(empty_library).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(duplicate_names).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(empty_name).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(unknown_default).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(missing_default).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(missing_terrain).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(short_color).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(future_format).has_value());
}

TEST(Biomes, InvertedClimateRangesAreRejected)
{
	nlohmann::json inverted = trok::BiomeLibrary::Serialize(TestBiomes());
	inverted["biomes"][0]["climate"]["moisture"] = { 0.5f, -0.5f };

	EXPECT_FALSE(Fs::DeserializeJson<trok::BiomeLibrary>(inverted).has_value());
}

TEST(Biomes, MissingClimateAxesCoverTheFullRange)
{
	nlohmann::json without_axis = trok::BiomeLibrary::Serialize(TestBiomes());
	without_axis["biomes"][0]["climate"].erase("temperature");

	const std::optional<trok::BiomeLibrary> library = Fs::DeserializeJson<trok::BiomeLibrary>(without_axis);

	ASSERT_TRUE(library.has_value());
	EXPECT_EQ(library->get(FLAT_BIOME).climate.temperature, trok::ClimateRange {});
}

TEST(Biomes, NamesResolveToIndices)
{
	const trok::BiomeLibrary library = TestBiomes();

	EXPECT_EQ(library.indexOf(FLAT.name), FLAT_BIOME);
	EXPECT_EQ(library.indexOf(TALL.name), TALL_BIOME);
	EXPECT_FALSE(library.indexOf(UNKNOWN_NAME).has_value());
	EXPECT_EQ(library.getNames(), (std::vector<std::string> { FLAT.name, TALL.name }));
}

TEST(Biomes, TheDefaultBiomeIsNamedByTheLibrary)
{
	EXPECT_EQ(TestBiomes().getDefault(), FLAT_BIOME);
	EXPECT_EQ(trok::BiomeLibrary::Create({ FLAT, TALL }, TALL.name)->getDefault(), TALL_BIOME);
}

TEST(Biomes, ClimatesInsideARangeSelectItsBiome)
{
	const trok::BiomeLibrary library = TestBiomes();

	EXPECT_EQ(library.select(INSIDE_FLAT, TIE_BREAKER), FLAT_BIOME);
	EXPECT_EQ(library.select(INSIDE_TALL, TIE_BREAKER), TALL_BIOME);
}

TEST(Biomes, ClimatesOutsideEveryRangeSelectTheNearestBiome)
{
	const trok::BiomeLibrary library = TestBiomes();

	EXPECT_EQ(library.select(NEARER_FLAT,  TIE_BREAKER), FLAT_BIOME);
	EXPECT_EQ(library.select(NEARER_TALL,  TIE_BREAKER), TALL_BIOME);
	EXPECT_EQ(library.select(COLD_LOWLAND, TIE_BREAKER), FLAT_BIOME);
}

TEST(Biomes, OverlappingRangesAreSharedByTheTieBreaker)
{
	const trok::BiomeLibrary library = OverlappingBiomes();

	std::set<trok::BiomeIndex> selected;
	for (uint64_t tie_breaker = 0; tie_breaker < TIE_BREAKER_COUNT; tie_breaker++)
		selected.insert(library.select(INSIDE_FLAT, tie_breaker));

	EXPECT_EQ(selected, (std::set<trok::BiomeIndex> { FLAT_BIOME, TALL_BIOME }));
	EXPECT_EQ(library.select(INSIDE_FLAT, TIE_BREAKER), library.select(INSIDE_FLAT, TIE_BREAKER));
}
