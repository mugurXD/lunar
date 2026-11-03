#pragma once
#include <trok/world/biome.hpp>
#include <trok/world/climate.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>

#include <lunar/world/decoration.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <cstdint>
#include <memory>
#include <vector>

class FastNoiseLite;

namespace trok
{
	using TerrainDecorator = lunar::World::TerrainDecorator<RegionPlan>;

	class BiomeDecorator final : public TerrainDecorator
	{
	public:
		BiomeDecorator(std::shared_ptr<const BiomeLibrary>                    biomes,
		               std::shared_ptr<const ClimateSampler>                  climate,
		               std::shared_ptr<const lunar::World::DecorationCatalog> catalog,
		               std::shared_ptr<const RoadLayer>                       roads,
		               int32_t                                                seed,
		               float                                                  sea_level) noexcept;
		~BiomeDecorator() noexcept override;

		void decorate(const RegionContext&                           context,
		              lunar::World::ChunkCoord                       coord,
		              const lunar::World::WorldSettings&             settings,
		              const lunar::World::Heightmap&                 heightmap,
		              std::vector<lunar::World::DecorationInstance>& output) const override;

	private:
		std::shared_ptr<const BiomeLibrary>                    biomes;
		std::shared_ptr<const ClimateSampler>                  climate;
		std::shared_ptr<const lunar::World::DecorationCatalog> catalog;
		std::shared_ptr<const RoadLayer>                       roads;
		int32_t                                                seed     = 0;
		float                                                  seaLevel = 0.f;
		std::unique_ptr<FastNoiseLite>                         clusters;
	};
}
