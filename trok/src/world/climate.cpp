#include <trok/world/climate.hpp>

#include "../../FastNoiseLite.h"

namespace trok
{
	namespace
	{
		constexpr int32_t TEMPERATURE_SEED_OFFSET     = 0;
		constexpr int32_t MOISTURE_SEED_OFFSET        = 7919;
		constexpr int32_t CONTINENTALNESS_SEED_OFFSET = 104729;
		constexpr int32_t EROSION_SEED_OFFSET         = 1299709;

		std::unique_ptr<FastNoiseLite> MakeNoise(int32_t seed, float frequency)
		{
			auto noise = std::make_unique<FastNoiseLite>(seed);
			noise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
			noise->SetFrequency(frequency);
			return noise;
		}
	}

	ClimateSampler::ClimateSampler(int32_t seed, const ClimateSettings& settings) noexcept
		: temperature(MakeNoise(seed + TEMPERATURE_SEED_OFFSET, settings.temperatureFrequency)),
		moisture(MakeNoise(seed + MOISTURE_SEED_OFFSET, settings.moistureFrequency)),
		continentalness(MakeNoise(seed + CONTINENTALNESS_SEED_OFFSET, settings.continentalnessFrequency)),
		erosion(MakeNoise(seed + EROSION_SEED_OFFSET, settings.erosionFrequency))
	{
	}

	ClimateSampler::~ClimateSampler() noexcept = default;

	Climate ClimateSampler::sample(double x, double z) const
	{
		return Climate
		{
			.temperature     = temperature->GetNoise(x, z),
			.moisture        = moisture->GetNoise(x, z),
			.continentalness = continentalness->GetNoise(x, z),
			.erosion         = erosion->GetNoise(x, z)
		};
	}
}
