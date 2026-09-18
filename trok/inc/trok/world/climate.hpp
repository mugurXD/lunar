#pragma once
#include <cstdint>
#include <memory>

class FastNoiseLite;

namespace trok
{
	struct Climate
	{
		float temperature     = 0.f;
		float moisture        = 0.f;
		float continentalness = 0.f;
		float erosion         = 0.f;

		bool operator==(const Climate&) const = default;
	};

	struct ClimateSettings
	{
		float temperatureFrequency     = 0.00004f;
		float moistureFrequency        = 0.00006f;
		float continentalnessFrequency = 0.00008f;
		float erosionFrequency         = 0.00015f;
	};

	class ClimateSampler
	{
	public:
		ClimateSampler(int32_t seed, const ClimateSettings& settings = {}) noexcept;
		~ClimateSampler() noexcept;

		Climate sample(double x, double z) const;

	private:
		std::unique_ptr<FastNoiseLite> temperature;
		std::unique_ptr<FastNoiseLite> moisture;
		std::unique_ptr<FastNoiseLite> continentalness;
		std::unique_ptr<FastNoiseLite> erosion;
	};
}
