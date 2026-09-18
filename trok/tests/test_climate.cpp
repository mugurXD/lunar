#include <trok/world/climate.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{
	constexpr int32_t WORLD_SEED     = 1337;
	constexpr int32_t OTHER_SEED     = 4242;
	constexpr double  SAMPLE_SPACING = 2500.0;
	constexpr int     SAMPLE_COUNT   = 64;

	std::vector<trok::Climate> SampleLine(const trok::ClimateSampler& sampler)
	{
		std::vector<trok::Climate> samples;
		for (int index = 0; index < SAMPLE_COUNT; index++)
			samples.push_back(sampler.sample(index * SAMPLE_SPACING, -index * SAMPLE_SPACING));

		return samples;
	}
}

TEST(Climate, SamplingIsDeterministicPerSeed)
{
	EXPECT_EQ(SampleLine(trok::ClimateSampler(WORLD_SEED)), SampleLine(trok::ClimateSampler(WORLD_SEED)));
	EXPECT_NE(SampleLine(trok::ClimateSampler(WORLD_SEED)), SampleLine(trok::ClimateSampler(OTHER_SEED)));
}

TEST(Climate, ValuesStayNormalised)
{
	for (const trok::Climate& climate : SampleLine(trok::ClimateSampler(WORLD_SEED)))
	{
		EXPECT_GE(climate.temperature,     -1.f);
		EXPECT_LE(climate.temperature,      1.f);
		EXPECT_GE(climate.moisture,        -1.f);
		EXPECT_LE(climate.moisture,         1.f);
		EXPECT_GE(climate.continentalness, -1.f);
		EXPECT_LE(climate.continentalness,  1.f);
	}
}

TEST(Climate, ValuesChangeOverLongDistances)
{
	const std::vector<trok::Climate> samples = SampleLine(trok::ClimateSampler(WORLD_SEED));

	EXPECT_FALSE(std::ranges::all_of(samples, [&samples](const trok::Climate& climate) { return climate == samples.front(); }));
}
