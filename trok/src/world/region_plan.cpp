#include <trok/world/region_plan.hpp>
#include <trok/world/stable_hash.hpp>

#include <lunar/debug.hpp>

#include <algorithm>

namespace trok
{
	namespace
	{
		constexpr uint32_t PLAN_FORMAT_VERSION = 2;

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

		double BiomeCellCenter(int32_t region_chunk, uint32_t cell, const lunar::World::WorldSettings& settings)
		{
			const int32_t chunk = region_chunk + static_cast<int32_t>(cell * BIOME_CELL_CHUNKS);
			return lunar::World::SampleCoordinate(chunk, 0, settings) + BiomeCellSize(settings) * 0.5;
		}

		uint64_t CellTieBreaker(int32_t seed, lunar::World::RegionCoord region, uint32_t cell_x, uint32_t cell_z, uint32_t cells_per_side)
		{
			const int64_t global_x = static_cast<int64_t>(region.x) * cells_per_side + cell_x;
			const int64_t global_z = static_cast<int64_t>(region.z) * cells_per_side + cell_z;

			return MixHash(MixHash(static_cast<uint64_t>(seed), static_cast<uint64_t>(global_x)), static_cast<uint64_t>(global_z));
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

	BiomeIndex RegionPlan::getBiome(uint32_t cell_x, uint32_t cell_z) const
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
			{ "biomePalette",      plan.biomePalette },
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
			.biomePalette      = json.at("biomePalette").get<std::vector<std::string>>(),
			.biomes            = json.at("biomes").get<std::vector<BiomeIndex>>()
		};

		const auto outside_palette = [&plan](BiomeIndex biome) { return biome >= plan.biomePalette.size(); };
		if (plan.biomes.size() != static_cast<size_t>(plan.biomeCellsPerSide) * plan.biomeCellsPerSide || std::ranges::any_of(plan.biomes, outside_palette))
			return std::nullopt;

		for (const nlohmann::json& settlement : json.at("settlements"))
			plan.settlements.push_back(DeserializeSettlement(settlement));

		return plan;
	}

	uint32_t BiomeCellsPerSide(const lunar::World::WorldSettings& settings)
	{
		return settings.regionChunks / BIOME_CELL_CHUNKS;
	}

	double BiomeCellSize(const lunar::World::WorldSettings& settings)
	{
		return static_cast<double>(BIOME_CELL_CHUNKS) * settings.getChunkSize();
	}

	std::optional<BiomeIndex> BiomeAt(const RegionContext& context, double x, double z)
	{
		const lunar::World::WorldSettings& settings = context.getSettings();
		const lunar::World::ChunkCoord     chunk    = lunar::World::ChunkAt(x, z, settings);
		const RegionPlan*                  plan     = context.findRegion(lunar::World::RegionAt(chunk, settings));
		if (plan == nullptr)
			return std::nullopt;

		const lunar::World::ChunkCoord local = lunar::World::ChunkWithinRegion(chunk, settings);
		return plan->getBiome(static_cast<uint32_t>(local.x) / BIOME_CELL_CHUNKS, static_cast<uint32_t>(local.z) / BIOME_CELL_CHUNKS);
	}

	RegionPlanner::RegionPlanner(uint32_t                              generator_version,
	                             int32_t                               seed,
	                             std::shared_ptr<const BiomeLibrary>   biomes,
	                             std::shared_ptr<const ClimateSampler> climate) noexcept
		: generatorVersion(generator_version),
		seed(seed),
		biomes(std::move(biomes)),
		climate(std::move(climate))
	{
	}

	RegionPlan RegionPlanner::plan(lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings) const
	{
		const uint32_t cells_per_side = BiomeCellsPerSide(settings);
		const int32_t  region_chunks  = static_cast<int32_t>(settings.regionChunks);

		RegionPlan plan =
		{
			.generatorVersion  = generatorVersion,
			.biomeCellsPerSide = cells_per_side,
			.biomePalette      = biomes->getNames(),
			.biomes            = std::vector<BiomeIndex>(static_cast<size_t>(cells_per_side) * cells_per_side, biomes->getDefault())
		};

		for (uint32_t cell_z = 0; cell_z < cells_per_side; cell_z++)
		{
			const double center_z = BiomeCellCenter(coord.z * region_chunks, cell_z, settings);

			for (uint32_t cell_x = 0; cell_x < cells_per_side; cell_x++)
			{
				const double   center_x    = BiomeCellCenter(coord.x * region_chunks, cell_x, settings);
				const uint64_t tie_breaker = CellTieBreaker(seed, coord, cell_x, cell_z, cells_per_side);

				plan.biomes[static_cast<size_t>(cell_z) * cells_per_side + cell_x] = biomes->select(climate->sample(center_x, center_z), tie_breaker);
			}
		}

		return plan;
	}

	RegionPlan RegionPlanner::restore(RegionPlan loaded, lunar::World::RegionCoord coord, const lunar::World::WorldSettings&) const
	{
		std::vector<BiomeIndex> library_indices;
		for (const std::string& name : loaded.biomePalette)
		{
			const std::optional<BiomeIndex> index = biomes->indexOf(name);
			if (!index.has_value())
			{
				DEBUG_WARN("Region ({}, {}) uses unknown biome '{}', replacing it with '{}'", coord.x, coord.z, name, biomes->get(biomes->getDefault()).name);
			}

			library_indices.push_back(index.value_or(biomes->getDefault()));
		}

		for (BiomeIndex& biome : loaded.biomes)
			biome = library_indices[biome];

		loaded.biomePalette = biomes->getNames();
		return loaded;
	}
}
