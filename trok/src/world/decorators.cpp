#include <trok/world/decorators.hpp>
#include <trok/world/stable_hash.hpp>

#include <lunar/debug.hpp>
#include <lunar/utils/geometry.hpp>
#include <lunar/world/terrain.hpp>

#include "../../FastNoiseLite.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <ranges>

namespace trok
{
	namespace
	{
		using lunar::World::DecorationInstance;
		using lunar::World::DecorationKind;

		constexpr uint64_t JITTER_X_SALT        = 1;
		constexpr uint64_t JITTER_Z_SALT        = 2;
		constexpr uint64_t CHANCE_SALT          = 3;
		constexpr uint64_t YAW_SALT             = 4;
		constexpr uint64_t SCALE_SALT           = 5;
		constexpr uint64_t CLUSTER_X_SALT       = 6;
		constexpr uint64_t CLUSTER_Z_SALT       = 7;
		constexpr size_t   CHUNK_CORNERS        = 4;
		constexpr size_t   CORNERS_PER_ROW      = 2;
		constexpr float    HALF                 = 0.5f;
		constexpr float    CLUSTER_SOFTNESS     = 0.1f;
		constexpr float    CLUSTER_FREQUENCY    = 1.f;
		constexpr double   CLUSTER_OFFSET_RANGE = 10000.0;
		constexpr float    MIN_TILT_AXIS        = 1e-6f;

		const glm::vec3 UP = { 0.f, 1.f, 0.f };

		struct Rule
		{
			const DecorationRule* definition = nullptr;
			BiomeIndex            biome      = 0;
			DecorationKind        kind       = 0;
			uint64_t              key        = 0;
		};

		struct Obstacle
		{
			glm::dvec2 from      = {};
			glm::dvec2 to        = {};
			double     halfWidth = 0.0;
		};

		struct Footprint
		{
			glm::vec2 position = {};
			float     radius   = 0.f;
		};

		struct Site
		{
			const lunar::World::WorldSettings&    settings;
			const lunar::World::Heightmap&        heightmap;
			const FastNoiseLite&                  clusters;
			int32_t                               seed       = 0;
			float                                 seaLevel   = 0.f;
			glm::dvec2                            minimum    = {};
			glm::dvec2                            maximum    = {};
			std::array<BiomeBlend, CHUNK_CORNERS> corners    = {};
			std::vector<Obstacle>                 obstacles  = {};
			std::shared_ptr<const RoadNetwork>    network    = nullptr;
			std::vector<Footprint>                footprints = {};
		};

		float Unit(uint64_t hash, uint64_t salt)
		{
			return static_cast<float>(UnitFromHash(MixHash(hash, salt)));
		}

		uint64_t CellHash(int32_t seed, int64_t cell_x, int64_t cell_z, uint64_t key)
		{
			return MixHash(MixHash(MixHash(static_cast<uint64_t>(seed), static_cast<uint64_t>(cell_x)), static_cast<uint64_t>(cell_z)), key);
		}

		float WeightIn(const BiomeBlend& blend, BiomeIndex biome)
		{
			for (size_t slot = 0; slot < blend.count; slot++)
				if (blend.indices[slot] == biome)
					return blend.weights[slot];

			return 0.f;
		}

		float BiomeWeightAt(const Site& site, BiomeIndex biome, const glm::vec2& amount)
		{
			const float near_row = glm::mix(WeightIn(site.corners[0], biome), WeightIn(site.corners[1], biome), amount.x);
			const float far_row  = glm::mix(WeightIn(site.corners[2], biome), WeightIn(site.corners[3], biome), amount.x);
			return glm::mix(near_row, far_row, amount.y);
		}

		std::vector<Rule> ActiveRules(const Site& site, const BiomeLibrary& biomes, const lunar::World::DecorationCatalog& catalog)
		{
			std::vector<BiomeIndex> present;
			for (const BiomeBlend& blend : site.corners)
				for (size_t slot = 0; slot < blend.count; slot++)
					present.push_back(blend.indices[slot]);

			std::ranges::sort(present);
			const auto [duplicates, end] = std::ranges::unique(present);
			present.erase(duplicates, end);

			std::vector<Rule> rules;
			for (const BiomeIndex biome : present)
			{
				const Biome& definition = biomes.get(biome);
				for (size_t index = 0; index < definition.decorations.size(); index++)
				{
					const DecorationRule&               rule = definition.decorations[index];
					const std::optional<DecorationKind> kind = catalog.find(rule.decoration);
					if (kind.has_value())
						rules.push_back({ &rule, biome, *kind, MixHash(MixHash(lunar::imp::fnv1a_hash(definition.name), lunar::imp::fnv1a_hash(rule.decoration)), index) });
				}
			}

			return rules;
		}

		std::vector<Obstacle> ObstaclesNear(const RegionContext& context, lunar::World::ChunkCoord coord, const Site& site, double reach)
		{
			const auto overlaps = [&](const glm::dvec2& from, const glm::dvec2& to, double half_width) {
				const glm::dvec2 margin = glm::dvec2(half_width + reach);
				return glm::all(glm::lessThanEqual(glm::min(from, to) - margin, site.maximum)) && glm::all(glm::greaterThanEqual(glm::max(from, to) + margin, site.minimum));
			};

			std::vector<Obstacle> obstacles;
			for (const lunar::World::RegionCoord region : lunar::World::RegionsNeededForChunk(coord, site.settings))
			{
				const RegionPlan* plan = context.findRegion(region);
				if (plan == nullptr)
					continue;

				for (const Settlement& settlement : plan->settlements)
					if (overlaps(settlement.center, settlement.center, settlement.radius))
						obstacles.push_back({ settlement.center, settlement.center, settlement.radius });

				for (const River& river : plan->rivers)
				{
					for (size_t point = 0; point + 1 < river.points.size(); point++)
					{
						const RiverPoint& from       = river.points[point];
						const RiverPoint& to         = river.points[point + 1];
						const double      half_width = std::max(from.width, to.width) * HALF;
						if (overlaps(from.position, to.position, half_width))
							obstacles.push_back({ from.position, to.position, half_width });
					}
				}
			}

			return obstacles;
		}

		float RoadReach(const RoadClass& road_class)
		{
			return road_class.halfWidth() + road_class.maxEdgeRun;
		}

		bool IsClear(const Site& site, const DecorationRule& rule, const glm::dvec2& position, const glm::vec2& local)
		{
			const bool crowded = rule.footprint > 0.f && std::ranges::any_of(site.footprints, [&](const Footprint& footprint) {
				return glm::distance(footprint.position, local) < footprint.radius + rule.footprint;
			});

			if (crowded)
				return false;

			const bool blocked = std::ranges::any_of(site.obstacles, [&](const Obstacle& obstacle) {
				double amount = 0.0;
				return lunar::DistanceToSegment(position, obstacle.from, obstacle.to, amount) < obstacle.halfWidth + rule.clearance;
			});

			if (blocked || site.network == nullptr)
				return !blocked;

			return !site.network->closestPoint(glm::vec2(position), RoadReach(site.network->getRoadClass()) + rule.clearance).has_value();
		}

		float ClusteringAt(const Site& site, const Rule& rule, const glm::dvec2& position)
		{
			const DecorationRule& definition = *rule.definition;
			if (definition.clusterFrequency <= 0.f)
				return 1.f;

			const glm::dvec2 offset = glm::dvec2(UnitFromHash(MixHash(rule.key, CLUSTER_X_SALT)), UnitFromHash(MixHash(rule.key, CLUSTER_Z_SALT))) * CLUSTER_OFFSET_RANGE;
			const glm::dvec2 sample = position * static_cast<double>(definition.clusterFrequency) + offset;
			const float      value  = site.clusters.GetNoise(sample.x, sample.y) * HALF + HALF;

			return glm::smoothstep(definition.clusterThreshold - CLUSTER_SOFTNESS, definition.clusterThreshold + CLUSTER_SOFTNESS, value);
		}

		glm::quat Orientation(uint64_t hash, const glm::vec3& normal, float tilt)
		{
			const glm::quat yaw  = glm::angleAxis(Unit(hash, YAW_SALT) * glm::two_pi<float>(), UP);
			const glm::vec3 lean = glm::normalize(glm::mix(UP, normal, tilt));
			const glm::vec3 axis = glm::cross(UP, lean);
			if (glm::length(axis) < MIN_TILT_AXIS)
				return yaw;

			return glm::angleAxis(std::acos(glm::clamp(lean.y, -1.f, 1.f)), glm::normalize(axis)) * yaw;
		}

		std::optional<DecorationInstance> TryPlace(Site& site, const Rule& rule, uint64_t hash, const glm::dvec2& position)
		{
			if (glm::any(glm::lessThan(position, site.minimum)) || glm::any(glm::greaterThanEqual(position, site.maximum)))
				return std::nullopt;

			const DecorationRule& definition = *rule.definition;
			const glm::vec2       local      = glm::vec2(position - site.minimum);
			const glm::vec2       amount     = glm::vec2((position - site.minimum) / (site.maximum - site.minimum));
			const float           chance     = definition.density * ClusteringAt(site, rule, position) * BiomeWeightAt(site, rule.biome, amount);
			if (Unit(hash, CHANCE_SALT) >= chance)
				return std::nullopt;

			const float      ground   = lunar::World::HeightmapHeightAt(site.heightmap, local, site.settings);
			const glm::ivec2 sample   = glm::ivec2(glm::round(local / site.settings.vertexSpacing));
			const glm::vec3  normal   = lunar::World::HeightmapNormal(site.heightmap, sample.x, sample.y, site.settings);
			const float      altitude = ground - site.seaLevel;
			if (altitude < definition.minAltitude || altitude > definition.maxAltitude || 1.f - normal.y > definition.maxSlope || !IsClear(site, definition, position, local))
				return std::nullopt;

			if (definition.footprint > 0.f)
				site.footprints.push_back({ local, definition.footprint });

			return DecorationInstance
			{
				.id       = hash,
				.kind     = rule.kind,
				.position = { local.x, ground - site.settings.maxFacetError, local.y },
				.rotation = Orientation(hash, normal, definition.tilt),
				.scale    = glm::mix(definition.minScale, definition.maxScale, Unit(hash, SCALE_SALT))
			};
		}

		void PlaceRule(Site& site, const Rule& rule, std::vector<DecorationInstance>& output)
		{
			const double  spacing = rule.definition->spacing;
			const int64_t first_x = static_cast<int64_t>(std::floor(site.minimum.x / spacing));
			const int64_t first_z = static_cast<int64_t>(std::floor(site.minimum.y / spacing));
			const int64_t last_x  = static_cast<int64_t>(std::floor(site.maximum.x / spacing));
			const int64_t last_z  = static_cast<int64_t>(std::floor(site.maximum.y / spacing));

			for (int64_t cell_z = first_z; cell_z <= last_z; cell_z++)
			{
				for (int64_t cell_x = first_x; cell_x <= last_x; cell_x++)
				{
					const uint64_t   hash     = CellHash(site.seed, cell_x, cell_z, rule.key);
					const glm::dvec2 jitter   = { UnitFromHash(MixHash(hash, JITTER_X_SALT)), UnitFromHash(MixHash(hash, JITTER_Z_SALT)) };
					const glm::dvec2 position = (glm::dvec2(static_cast<double>(cell_x), static_cast<double>(cell_z)) + jitter) * spacing;

					if (std::optional<DecorationInstance> instance = TryPlace(site, rule, hash, position))
						output.push_back(*instance);
				}
			}
		}
	}

	BiomeDecorator::BiomeDecorator(std::shared_ptr<const BiomeLibrary>                    biomes,
	                               std::shared_ptr<const ClimateSampler>                  climate,
	                               std::shared_ptr<const lunar::World::DecorationCatalog> catalog,
	                               std::shared_ptr<const RoadLayer>                       roads,
	                               int32_t                                                seed,
	                               float                                                  sea_level) noexcept
		: biomes(std::move(biomes)),
		climate(std::move(climate)),
		catalog(std::move(catalog)),
		roads(std::move(roads)),
		seed(seed),
		seaLevel(sea_level),
		clusters(std::make_unique<FastNoiseLite>(seed))
	{
		clusters->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		clusters->SetFrequency(CLUSTER_FREQUENCY);

		for (const Biome& biome : this->biomes->getBiomes())
			for (const DecorationRule& rule : biome.decorations)
				if (!this->catalog->find(rule.decoration).has_value())
					DEBUG_WARN("Biome '{}' places '{}', which is not a known decoration", biome.name, rule.decoration);
	}

	BiomeDecorator::~BiomeDecorator() noexcept = default;

	void BiomeDecorator::decorate(const RegionContext&                           context,
	                              lunar::World::ChunkCoord                       coord,
	                              const lunar::World::WorldSettings&             settings,
	                              const lunar::World::Heightmap&                 heightmap,
	                              std::vector<lunar::World::DecorationInstance>& output) const
	{
		const glm::dvec2 minimum = { lunar::World::SampleCoordinate(coord.x, 0, settings), lunar::World::SampleCoordinate(coord.z, 0, settings) };
		const glm::dvec2 maximum = minimum + glm::dvec2(settings.getChunkSize());

		Site site = { .settings = settings, .heightmap = heightmap, .clusters = *clusters, .seed = seed, .seaLevel = seaLevel, .minimum = minimum, .maximum = maximum };
		for (size_t corner = 0; corner < CHUNK_CORNERS; corner++)
		{
			const glm::dvec2 point = glm::mix(minimum, maximum, glm::dvec2(static_cast<double>(corner % CORNERS_PER_ROW), static_cast<double>(corner / CORNERS_PER_ROW)));
			site.corners[corner] = biomes->blendAt(climate->sample(point.x, point.y));
		}

		const std::vector<Rule> rules = ActiveRules(site, *biomes, *catalog);
		if (rules.empty())
			return;

		const float reach = std::ranges::max(rules | std::views::transform([](const Rule& rule) { return rule.definition->clearance; }));
		site.obstacles = ObstaclesNear(context, coord, site, reach);
		site.network   = roads == nullptr ? nullptr : roads->get();

		for (const Rule& rule : rules)
			PlaceRule(site, rule, output);
	}
}
