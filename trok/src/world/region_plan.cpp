#include <trok/world/region_plan.hpp>

namespace trok
{
	namespace
	{
		constexpr uint32_t PLAN_FORMAT_VERSION = 1;

		nlohmann::json SerializeSettlement(const Settlement& settlement)
		{
			return nlohmann::json
			{
				{ "id",     settlement.id },
				{ "type",   settlement.type },
				{ "center", { settlement.center.x, settlement.center.y } },
				{ "radius", settlement.radius }
			};
		}

		Settlement DeserializeSettlement(const nlohmann::json& json)
		{
			const nlohmann::json& center = json.at("center");

			return Settlement
			{
				.id     = json.at("id").get<uint32_t>(),
				.type   = json.at("type").get<std::string>(),
				.center = { center.at(0).get<double>(), center.at(1).get<double>() },
				.radius = json.at("radius").get<double>()
			};
		}
	}

	BiomeId RegionPlan::getBiome(uint32_t cell_x, uint32_t cell_z) const
	{
		return biomes[static_cast<size_t>(cell_z) * biomeCellsPerSide + cell_x];
	}

	nlohmann::json RegionPlan::Serialize(const RegionPlan& plan)
	{
		nlohmann::json settlements = nlohmann::json::array();
		for (const Settlement& settlement : plan.settlements)
			settlements.push_back(SerializeSettlement(settlement));

		return nlohmann::json
		{
			{ "formatVersion",     PLAN_FORMAT_VERSION },
			{ "generatorVersion",  plan.generatorVersion },
			{ "biomeCellsPerSide", plan.biomeCellsPerSide },
			{ "biomes",            plan.biomes },
			{ "settlements",       std::move(settlements) }
		};
	}

	std::optional<RegionPlan> RegionPlan::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != PLAN_FORMAT_VERSION)
			return std::nullopt;

		RegionPlan plan =
		{
			.generatorVersion  = json.at("generatorVersion").get<uint32_t>(),
			.biomeCellsPerSide = json.at("biomeCellsPerSide").get<uint32_t>(),
			.biomes            = json.at("biomes").get<std::vector<BiomeId>>()
		};

		if (plan.biomes.size() != static_cast<size_t>(plan.biomeCellsPerSide) * plan.biomeCellsPerSide)
			return std::nullopt;

		for (const nlohmann::json& settlement : json.at("settlements"))
			plan.settlements.push_back(DeserializeSettlement(settlement));

		return plan;
	}

	uint32_t BiomeCellsPerSide(const lunar::World::WorldSettings& settings)
	{
		return settings.regionChunks / BIOME_CELL_CHUNKS;
	}

	BiomeId BiomeAt(const RegionContext& context, double x, double z)
	{
		const lunar::World::WorldSettings& settings = context.getSettings();
		const lunar::World::ChunkCoord     chunk    = lunar::World::ChunkAt(x, z, settings);
		const RegionPlan*                  plan     = context.findRegion(lunar::World::RegionAt(chunk, settings));
		if (plan == nullptr)
			return DEFAULT_BIOME;

		const lunar::World::ChunkCoord local = lunar::World::ChunkWithinRegion(chunk, settings);
		return plan->getBiome(static_cast<uint32_t>(local.x) / BIOME_CELL_CHUNKS, static_cast<uint32_t>(local.z) / BIOME_CELL_CHUNKS);
	}

	RegionPlanner::RegionPlanner(uint32_t generator_version) noexcept
		: generatorVersion(generator_version)
	{
	}

	RegionPlan RegionPlanner::plan(lunar::World::RegionCoord, const lunar::World::WorldSettings& settings) const
	{
		const uint32_t cells_per_side = BiomeCellsPerSide(settings);

		return RegionPlan
		{
			.generatorVersion  = generatorVersion,
			.biomeCellsPerSide = cells_per_side,
			.biomes            = std::vector<BiomeId>(static_cast<size_t>(cells_per_side) * cells_per_side, DEFAULT_BIOME)
		};
	}
}
