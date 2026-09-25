#include <trok/world/terrain_generator.hpp>
#include <trok/world/stable_hash.hpp>

#include "../../FastNoiseLite.h"

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		std::unique_ptr<FastNoiseLite> MakeNoise(int32_t seed, const BiomeTerrain& terrain)
		{
			auto noise = std::make_unique<FastNoiseLite>(seed);
			noise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
			noise->SetFractalType(terrain.ridged ? FastNoiseLite::FractalType_Ridged : FastNoiseLite::FractalType_FBm);
			noise->SetFractalOctaves(terrain.octaves);
			noise->SetFrequency(terrain.frequency);
			return noise;
		}
	}

	TerrainGenerator::TerrainGenerator(std::shared_ptr<const BiomeLibrary>   biomes,
	                                   std::shared_ptr<const ClimateSampler> climate,
	                                   ElevationCurve                        elevation,
	                                   int32_t                               seed) noexcept
		: biomes(std::move(biomes)),
		seed(seed),
		climate(std::move(climate)),
		elevation(std::move(elevation))
	{
		refresh();
	}

	void TerrainGenerator::refresh()
	{
		noises.clear();
		for (const Biome& biome : biomes->getBiomes())
			noises.push_back(MakeNoise(SeedFromHash(MixHash(static_cast<uint64_t>(seed), lunar::imp::fnv1a_hash(biome.name))), biome.terrain));
	}

	TerrainGenerator::~TerrainGenerator() noexcept = default;

	float TerrainGenerator::sampleBaseHeight(const RegionContext&, double x, double z) const
	{
		const Climate    climate_here = climate->sample(x, z);
		const BiomeBlend blend        = biomes->blendAt(climate_here);

		float height = elevation.heightAt(climate_here.continentalness);
		for (size_t slot = 0; slot < blend.count; slot++)
			height += blend.weights[slot] * biomeHeight(blend.indices[slot], x, z);

		return height;
	}

	glm::vec3 TerrainGenerator::sampleColor(const RegionContext&, double x, double z, float height, const glm::vec3& normal) const
	{
		const Climate    climate_here = climate->sample(x, z);
		const BiomeBlend blend        = biomes->blendAt(climate_here);
		const float      local_height = height - elevation.heightAt(climate_here.continentalness);

		glm::vec3 color = {};
		for (size_t slot = 0; slot < blend.count; slot++)
			color += blend.weights[slot] * biomeColor(blend.indices[slot], local_height, normal);

		return color;
	}

	float TerrainGenerator::biomeHeight(BiomeIndex biome, double x, double z) const
	{
		const BiomeTerrain& terrain = biomes->get(biome).terrain;
		return terrain.heightOffset + noises[biome]->GetNoise(x, z) * terrain.amplitude;
	}

	glm::vec3 TerrainGenerator::biomeColor(BiomeIndex biome, float local_height, const glm::vec3& normal) const
	{
		const Biome&        definition    = biomes->get(biome);
		const BiomeColors&  colors        = definition.colors;
		const BiomeTerrain& terrain       = definition.terrain;
		const float         height_factor = glm::smoothstep(terrain.heightOffset - terrain.amplitude, terrain.heightOffset + terrain.amplitude, local_height);
		const glm::vec3     ground        = glm::mix(colors.lowColor, colors.highColor, height_factor);
		const float         rock_factor   = glm::smoothstep(colors.rockSlopeStart, colors.rockSlopeEnd, 1.f - normal.y);

		return glm::mix(ground, colors.rockColor, rock_factor);
	}
}
