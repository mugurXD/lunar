#include <trok/world/region_plan.hpp>
#include <trok/world/stable_hash.hpp>

#include <lunar/debug.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace trok
{
	namespace
	{
		constexpr uint32_t         PLAN_FORMAT_VERSION    = 3;
		constexpr double           HALF                   = 0.5;
		constexpr double           SETTLEMENT_JITTER      = 0.5;
		constexpr double           SETTLEMENT_MIN_RADIUS  = 140.0;
		constexpr double           SETTLEMENT_MAX_RADIUS  = 380.0;
		constexpr uint64_t         SETTLEMENT_OFFSET_SALT = 1;
		constexpr uint64_t         SETTLEMENT_RADIUS_SALT = 2;
		constexpr std::string_view SETTLEMENT_TYPE        = "core:town";
		constexpr int              SETTLEMENT_EDGE_PROBES = 8;

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

		double UnitFromHash(uint64_t hash)
		{
			return static_cast<double>(hash >> 11) * 0x1p-53;
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

		nlohmann::json rivers = nlohmann::json::array();
		for (const River& river : plan.rivers)
			rivers.push_back(River::Serialize(river));

		return nlohmann::json
		{
			{ "formatVersion",     PLAN_FORMAT_VERSION },
			{ "generatorVersion",  plan.generatorVersion },
			{ "biomeCellsPerSide", plan.biomeCellsPerSide },
			{ "biomePalette",      plan.biomePalette },
			{ "biomes",            plan.biomes },
			{ "settlements",       std::move(settlements) },
			{ "rivers",            std::move(rivers) }
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

		for (const nlohmann::json& river : json.at("rivers"))
		{
			const std::optional<River> restored = River::Deserialize(river);
			if (!restored.has_value())
				return std::nullopt;

			plan.rivers.push_back(*restored);
		}

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
	                             std::shared_ptr<const ClimateSampler> climate,
	                             ElevationCurve                        elevation) noexcept
		: generatorVersion(generator_version),
		seed(seed),
		biomes(std::move(biomes)),
		climate(std::move(climate)),
		elevation(std::move(elevation))
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

		traceRivers(plan, coord, settings);
		placeSettlement(plan, coord, settings);
		return plan;
	}

	void RegionPlanner::traceRivers(RegionPlan& plan, lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings) const
	{
		//const int32_t    region_chunks = static_cast<int32_t>(settings.regionChunks);
		//const double     region_size   = static_cast<double>(settings.regionChunks) * settings.getChunkSize();
		//const double     block_size    = region_size * RIVER_SOURCE_REGIONS;
		//const glm::dvec2 origin        = { lunar::World::SampleCoordinate(coord.x * region_chunks, 0, settings),
		//                                   lunar::World::SampleCoordinate(coord.z * region_chunks, 0, settings) };

		//const double     margin  = RiverReach() * 2.0;
		//const glm::dvec2 minimum = origin - glm::dvec2(margin);
		//const glm::dvec2 maximum = origin + glm::dvec2(region_size + margin);
		//const glm::dvec2 middle  = origin + glm::dvec2(region_size * HALF);

		//const int32_t block_x = static_cast<int32_t>(std::floor(origin.x / block_size));
		//const int32_t block_z = static_cast<int32_t>(std::floor(origin.y / block_size));

		//for (int32_t z = -1; z <= 1; z++)
		//{
		//	for (int32_t x = -1; x <= 1; x++)
		//	{
		//		const glm::dvec2 source = RiverSource(seed, block_x + x, block_z + z, block_size);
		//		if (glm::distance(source, middle) > RiverMaxLength() + region_size)
		//			continue;

		//		const uint64_t id     = MixHash(static_cast<uint64_t>(block_x + x), static_cast<uint64_t>(block_z + z));
		//		const River    traced = TraceRiver(static_cast<uint32_t>(id), source, elevation, *climate, *biomes);
		//		if (traced.points.empty())
		//			continue;

		//		const River clipped = ClipRiver(traced, minimum, maximum);
		//		if (clipped.points.size() >= 2)
		//			plan.rivers.push_back(clipped);
		//	}
		//}
	}

	void RegionPlanner::placeSettlement(RegionPlan& plan, lunar::World::RegionCoord coord, const lunar::World::WorldSettings& settings) const
	{
		const int32_t    region_chunks = static_cast<int32_t>(settings.regionChunks);
		const double     region_size   = static_cast<double>(settings.regionChunks) * settings.getChunkSize();
		const glm::dvec2 origin        = { lunar::World::SampleCoordinate(coord.x * region_chunks, 0, settings),
		                                   lunar::World::SampleCoordinate(coord.z * region_chunks, 0, settings) };

		const uint64_t   roll   = MixHash(MixHash(static_cast<uint64_t>(seed), static_cast<uint64_t>(coord.x)), static_cast<uint64_t>(coord.z));
		const glm::dvec2 offset = { UnitFromHash(roll) - HALF, UnitFromHash(MixHash(roll, SETTLEMENT_OFFSET_SALT)) - HALF };
		const glm::dvec2 center = origin + region_size * (glm::dvec2(HALF) + offset * SETTLEMENT_JITTER);

		const double cell_size = BiomeCellSize(settings);
		const double radius    = glm::mix(SETTLEMENT_MIN_RADIUS, SETTLEMENT_MAX_RADIUS, UnitFromHash(MixHash(roll, SETTLEMENT_RADIUS_SALT)));

		const auto biome_at = [&](const glm::dvec2& point) -> const Biome& {
			const glm::uvec2 cell = glm::min(glm::uvec2((point - origin) / cell_size), glm::uvec2(plan.biomeCellsPerSide - 1));
			return biomes->get(plan.getBiome(cell.x, cell.y));
		};

		const auto may_flood = [&](const glm::dvec2& point) {
			const Climate      local   = climate->sample(point.x, point.y);
			const BiomeTerrain terrain = biomes->terrainAt(local);
			return elevation.heightAt(local.continentalness) + terrain.heightOffset - terrain.amplitude <= elevation.seaLevel;
		};

		if (!biome_at(center).habitable || may_flood(center))
			return;

		for (int probe = 0; probe < SETTLEMENT_EDGE_PROBES; probe++)
		{
			const double angle = glm::two_pi<double>() * probe / SETTLEMENT_EDGE_PROBES;
			if (may_flood(center + glm::dvec2(std::cos(angle), std::sin(angle)) * radius))
				return;
		}

		plan.settlements.push_back({
			.id     = static_cast<uint32_t>(roll),
			.type   = std::string(SETTLEMENT_TYPE),
			.center = center,
			.radius = radius
		});
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
