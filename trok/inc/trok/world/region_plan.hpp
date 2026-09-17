#pragma once
#include <lunar/world/grid.hpp>
#include <lunar/world/region.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace trok
{
	using BiomeId = uint16_t;

	constexpr BiomeId  DEFAULT_BIOME     = 0;
	constexpr uint32_t BIOME_CELL_CHUNKS = 4;

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
		uint32_t                generatorVersion  = 0;
		uint32_t                biomeCellsPerSide = 0;
		std::vector<BiomeId>    biomes            = {};
		std::vector<Settlement> settlements       = {};

		BiomeId getBiome(uint32_t cell_x, uint32_t cell_z) const;

		bool operator==(const RegionPlan&) const = default;

		static nlohmann::json            Serialize(const RegionPlan& plan);
		static std::optional<RegionPlan> Deserialize(const nlohmann::json& json);
	};

	using RegionContext = lunar::World::RegionContext<RegionPlan>;

	uint32_t BiomeCellsPerSide(const lunar::World::WorldSettings& settings);
	BiomeId  BiomeAt(const RegionContext& context, double x, double z);

	class RegionPlanner final : public lunar::World::RegionPlanner<RegionPlan>
	{
	public:
		RegionPlanner(uint32_t generator_version) noexcept;

		RegionPlan plan(lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings) const override;

	private:
		uint32_t generatorVersion = 0;
	};
}
