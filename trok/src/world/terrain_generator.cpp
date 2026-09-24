#include <trok/world/terrain_generator.hpp>
#include <trok/world/stable_hash.hpp>

#include "../../FastNoiseLite.h"

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr double CELL_CENTER = 0.5;

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
	                                   int32_t                               seed,
	                                   std::shared_ptr<const RoadLayer>      roads) noexcept
		: biomes(std::move(biomes)),
		seed(seed),
		climate(std::move(climate)),
		roads(std::move(roads)),
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

	float TerrainGenerator::sampleBaseHeight(const RegionContext& context, double x, double z) const
	{
		const Blend blend = gatherBlend(context, x, z);

		float height = elevationAt(x, z);
		for (size_t corner = 0; corner < BIOME_BLEND_CELLS; corner++)
			if (blend.weights[corner] > 0.f)
				height += blend.weights[corner] * biomeHeight(blend.indices[corner], x, z);

		const RoadNetwork* network = roads == nullptr ? nullptr : roads->get();
		return network == nullptr ? height : network->gradedHeight(x, z, height);
	}

	glm::vec3 TerrainGenerator::sampleColor(const RegionContext& context, double x, double z, float height, const glm::vec3& normal) const
	{
		const Blend blend        = gatherBlend(context, x, z);
		const float local_height = height - elevationAt(x, z);

		glm::vec3 color = {};
		for (size_t corner = 0; corner < BIOME_BLEND_CELLS; corner++)
			if (blend.weights[corner] > 0.f)
				color += blend.weights[corner] * biomeColor(blend.indices[corner], local_height, normal);

		return color;
	}

	TerrainGenerator::Blend TerrainGenerator::gatherBlend(const RegionContext& context, double x, double z) const
	{
		const double cell_size  = BiomeCellSize(context.getSettings());
		const double grid_x     = x / cell_size - CELL_CENTER;
		const double grid_z     = z / cell_size - CELL_CENTER;
		const double base_x     = std::floor(grid_x);
		const double base_z     = std::floor(grid_z);
		const float  fraction_x = static_cast<float>(grid_x - base_x);
		const float  fraction_z = static_cast<float>(grid_z - base_z);

		Blend blend = {};
		for (size_t corner = 0; corner < BIOME_BLEND_CELLS; corner++)
		{
			const size_t                    offset_x = corner % 2;
			const size_t                    offset_z = corner / 2;
			const std::optional<BiomeIndex> biome    = BiomeAt(context, (base_x + offset_x + CELL_CENTER) * cell_size,
			                                                            (base_z + offset_z + CELL_CENTER) * cell_size);

			const BiomeIndex index  = biome.value_or(biomes->getDefault());
			const float      weight = (offset_x == 0 ? 1.f - fraction_x : fraction_x) * (offset_z == 0 ? 1.f - fraction_z : fraction_z);
			const auto       first  = std::ranges::find(blend.indices.begin(), blend.indices.begin() + corner, index);

			blend.indices[corner] = index;
			blend.weights[static_cast<size_t>(first - blend.indices.begin())] += weight;
		}

		return blend;
	}

	float TerrainGenerator::elevationAt(double x, double z) const
	{
		return elevation.heightAt(climate->sampleContinentalness(x, z));
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
