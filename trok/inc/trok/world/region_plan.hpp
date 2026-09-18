#pragma once
#include <trok/world/biome.hpp>
#include <trok/world/climate.hpp>

#include <lunar/world/grid.hpp>
#include <lunar/world/region.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace trok
{
	constexpr uint32_t BIOME_CELL_CHUNKS         = 4;
	constexpr int32_t  BIOME_SAMPLE_REACH_CHUNKS = static_cast<int32_t>(BIOME_CELL_CHUNKS) + 1;

	struct Settlement
	{
		uint32_t    id     = 0;
		std::string type   = {};
		glm::dvec2  center = {};
		double      radius = 0.0;

		bool operator==(const Settlement&) const = default;
	};

	struct RegionPlan
	{
		uint32_t                 generatorVersion  = 0;
		uint32_t                 biomeCellsPerSide = 0;
		std::vector<std::string> biomePalette      = {};
		std::vector<BiomeIndex>  biomes            = {};
		std::vector<Settlement>  settlements       = {};

		BiomeIndex getBiome(uint32_t cell_x, uint32_t cell_z) const;

		bool operator==(const RegionPlan&) const = default;

		static nlohmann::json            Serialize(const RegionPlan& plan);
		static std::optional<RegionPlan> Deserialize(const nlohmann::json& json);
	};

	using RegionContext = lunar::World::RegionContext<RegionPlan>;

	uint32_t                  BiomeCellsPerSide(const lunar::World::WorldSettings& settings);
	double                    BiomeCellSize(const lunar::World::WorldSettings& settings);
	std::optional<BiomeIndex> BiomeAt(const RegionContext& context, double x, double z);

	class RegionPlanner final : public lunar::World::RegionPlanner<RegionPlan>
	{
	public:
		RegionPlanner(uint32_t                              generator_version,
		              int32_t                               seed,
		              std::shared_ptr<const BiomeLibrary>   biomes,
		              std::shared_ptr<const ClimateSampler> climate) noexcept;

		RegionPlan plan(lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings)                       const override;
		RegionPlan restore(RegionPlan loaded, lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings) const override;

	private:
		uint32_t                              generatorVersion = 0;
		int32_t                               seed             = 0;
		std::shared_ptr<const BiomeLibrary>   biomes           = nullptr;
		std::shared_ptr<const ClimateSampler> climate          = nullptr;
	};
}
