#include <trok/world/terrain_generator.hpp>
#include <lunar/world/terrain.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

namespace
{
	constexpr int32_t OTHER_SEED     = 4242;
	constexpr double  SAMPLE_SPACING = 37.5;
	constexpr int     SAMPLE_COUNT   = 200;

	const lunar::World::WorldSettings SETTINGS = {};
	const trok::RegionContext         CONTEXT  = trok::RegionContext(SETTINGS, {});
	const glm::vec3                   UP       = { 0.f, 1.f, 0.f };
	const glm::vec3                   CLIFF    = { 1.f, 0.f, 0.f };

	lunar::World::Heightmap ChunkHeights(const trok::TerrainGenerator& generator, lunar::World::ChunkCoord coord)
	{
		return lunar::World::SampleHeightmap([&](double x, double z) { return generator.sampleHeight(CONTEXT, x, z); }, coord, SETTINGS);
	}
}

TEST(TrokTerrain, GenerationIsDeterministicPerSeed)
{
	const trok::TerrainGenerator generator({});
	const trok::TerrainGenerator same_seed({});
	const trok::TerrainGenerator other_seed({ .seed = OTHER_SEED });
	const lunar::World::ChunkCoord coord = { 2, 7 };

	EXPECT_EQ(ChunkHeights(generator, coord), ChunkHeights(same_seed, coord));
	EXPECT_NE(ChunkHeights(generator, coord), ChunkHeights(other_seed, coord));
}

TEST(TrokTerrain, HeightsStayWithinTheAmplitude)
{
	const trok::TerrainSettings  settings = {};
	const trok::TerrainGenerator generator(settings);

	for (int index = 0; index < SAMPLE_COUNT; index++)
	{
		const float height = generator.sampleHeight(CONTEXT, index * SAMPLE_SPACING, -index * SAMPLE_SPACING);
		EXPECT_LE(height,  settings.amplitude);
		EXPECT_GE(height, -settings.amplitude);
	}
}

TEST(TrokTerrain, SteepSlopesAreRock)
{
	const trok::TerrainSettings  settings = {};
	const trok::TerrainGenerator generator(settings);

	EXPECT_EQ(generator.sampleColor(CONTEXT, 0.0, 0.0, 0.f, CLIFF), settings.rockColor);
	EXPECT_NE(generator.sampleColor(CONTEXT, 0.0, 0.0, 0.f, UP),    settings.rockColor);
}
