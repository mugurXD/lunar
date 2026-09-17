#include <trok/world/terrain_generator.hpp>

#include "../../FastNoiseLite.h"

namespace trok
{
	TerrainGenerator::TerrainGenerator(const TerrainSettings& settings) noexcept
		: settings(settings),
		noise(std::make_unique<FastNoiseLite>(settings.seed))
	{
		noise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		noise->SetFractalType(FastNoiseLite::FractalType_FBm);
		noise->SetFractalOctaves(settings.octaves);
		noise->SetFrequency(settings.frequency);
	}

	TerrainGenerator::~TerrainGenerator() noexcept = default;

	float TerrainGenerator::sampleHeight(const RegionContext&, double x, double z) const
	{
		return noise->GetNoise(x, z) * settings.amplitude;
	}

	glm::vec3 TerrainGenerator::sampleColor(const RegionContext&, double, double, float height, const glm::vec3& normal) const
	{
		const float     height_factor = glm::smoothstep(-settings.amplitude, settings.amplitude, height);
		const glm::vec3 grass         = glm::mix(settings.lowGrassColor, settings.highGrassColor, height_factor);
		const float     rock_factor   = glm::smoothstep(settings.rockSlopeStart, settings.rockSlopeEnd, 1.f - normal.y);

		return glm::mix(grass, settings.rockColor, rock_factor);
	}
}
