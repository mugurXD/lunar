#pragma once
#include <trok/world/climate.hpp>
#include <trok/world/elevation.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class FastNoiseLite;

namespace trok
{
	constexpr size_t BIOME_BLEND_CELLS = 4;
	constexpr size_t QUAD_CORNERS      = 4;

	class TerrainGenerator final : public lunar::World::TerrainGenerator<RegionPlan>
	{
	public:
		TerrainGenerator(std::shared_ptr<const BiomeLibrary>   biomes,
		                 std::shared_ptr<const ClimateSampler> climate,
		                 ElevationCurve                        elevation,
		                 int32_t                               seed) noexcept;
		~TerrainGenerator() noexcept override;

		void      refresh();
		void      setRoads(std::shared_ptr<const RoadNetwork> network);
		float     sampleHeight(const RegionContext& context, double x, double z)                                       const override;
		glm::vec3 sampleColor(const RegionContext& context, double x, double z, float height, const glm::vec3& normal) const override;
		void      buildDecorations(const RegionContext& context, lunar::World::ChunkCoord coord, const lunar::World::WorldSettings& settings, lunar::Render::MeshData& mesh) const override;

	private:
		struct Blend
		{
			std::array<BiomeIndex, BIOME_BLEND_CELLS> indices = {};
			std::array<float,      BIOME_BLEND_CELLS> weights = {};
		};

		static void appendQuad(lunar::Render::MeshData& mesh, const std::array<glm::vec3, QUAD_CORNERS>& corners, const glm::vec3& origin, const glm::vec3& color);

		glm::vec3 baseEdge(const RegionContext& context, const glm::vec3& top, const glm::vec3& outward) const;
		Blend     gatherBlend(const RegionContext& context, double x, double z)             const;
		float     elevationAt(double x, double z)                                           const;
		float     biomeHeight(BiomeIndex biome, double x, double z)                         const;
		glm::vec3 biomeColor(BiomeIndex biome, float local_height, const glm::vec3& normal) const;

		std::shared_ptr<const BiomeLibrary>         biomes;
		int32_t                                     seed = 0;
		std::shared_ptr<const ClimateSampler>       climate;
		std::shared_ptr<const RoadNetwork>          roads;
		ElevationCurve                              elevation;
		std::vector<std::unique_ptr<FastNoiseLite>> noises;
	};
}
