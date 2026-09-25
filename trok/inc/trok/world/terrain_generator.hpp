#pragma once
#include <trok/world/climate.hpp>
#include <trok/world/elevation.hpp>
#include <trok/world/region_plan.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class FastNoiseLite;

namespace trok
{
	class TerrainGenerator final : public lunar::World::TerrainGenerator<RegionPlan>
	{
	public:
		TerrainGenerator(std::shared_ptr<const BiomeLibrary>   biomes,
		                 std::shared_ptr<const ClimateSampler> climate,
		                 ElevationCurve                        elevation,
		                 int32_t                               seed) noexcept;
		~TerrainGenerator() noexcept override;

		void      refresh();
		float     sampleBaseHeight(const RegionContext& context, double x, double z)                                   const override;
		glm::vec3 sampleColor(const RegionContext& context, double x, double z, float height, const glm::vec3& normal) const override;

	private:
		float     biomeHeight(BiomeIndex biome, double x, double z)                         const;
		glm::vec3 biomeColor(BiomeIndex biome, float local_height, const glm::vec3& normal) const;

		std::shared_ptr<const BiomeLibrary>         biomes;
		int32_t                                     seed = 0;
		std::shared_ptr<const ClimateSampler>       climate;
		ElevationCurve                              elevation;
		std::vector<std::unique_ptr<FastNoiseLite>> noises;
	};
}
