#include <trok/world/terrain_generator.hpp>
#include <trok/world/stable_hash.hpp>

#include "../../FastNoiseLite.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace trok
{
	namespace
	{
		constexpr double CELL_CENTER     = 0.5;
		constexpr float  EMBANKMENT_SINK = 0.1f;
		constexpr float  EMBANKMENT_STEP = 1.f;
		constexpr float  WATER_ALPHA     = 0.55f;
		constexpr uint32_t OCEAN_PROBES  = 4;

		const glm::vec3  WATER_COLOR     = { 0.13f, 0.28f, 0.42f };

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
	                                   int32_t                               seed) noexcept
		: biomes(std::move(biomes)),
		seed(seed),
		climate(std::move(climate)),
		elevation(std::move(elevation))
	{
		refresh();
	}

	void TerrainGenerator::setRoads(std::shared_ptr<const RoadNetwork> network)
	{
		roads = std::move(network);
	}

	void TerrainGenerator::appendQuad(lunar::Render::MeshData& mesh, const std::array<glm::vec3, QUAD_CORNERS>& corners, const glm::vec3& origin, const glm::vec3& color, float alpha)
	{
		const auto      first  = static_cast<uint32_t>(mesh.vertices.size());
		const glm::vec3 facing = glm::normalize(glm::cross(corners[2] - corners[0], corners[1] - corners[0]));
		const glm::vec3 normal = facing.y < 0.f ? -facing : facing;

		for (const glm::vec3& corner : corners)
			mesh.vertices.push_back({ .position = corner - origin, .normal = normal, .color = glm::vec4(color, alpha) });

		for (const uint32_t index : { 0u, 2u, 1u, 1u, 2u, 3u })
			mesh.indices.push_back(first + index);
	}

	glm::vec3 TerrainGenerator::baseEdge(const RegionContext& context, const glm::vec3& top, const glm::vec3& outward) const
	{
		const RoadClass& road_class = roads->getRoadClass();
		const glm::vec3  direction  = outward / road_class.halfWidth();
		const float      spread     = road_class.embankmentSpread;
		const float      widest     = road_class.maxGradingWidth - road_class.halfWidth();
		const float      deepest    = std::max(spread > 0.f ? std::min(road_class.maxFillHeight, widest / spread) : road_class.maxFillHeight,
		                                       road_class.edgeDepth);

		float drop      = road_class.edgeDepth;
		float closest   = std::numeric_limits<float>::max();
		float best_drop = drop;

		while (true)
		{
			const glm::vec3 point = top + direction * (drop * spread);
			const float     gap   = top.y - drop - sampleHeight(context, point.x, point.z);

			if (gap <= 0.f)
				return { point.x, top.y - drop - EMBANKMENT_SINK, point.z };

			if (gap < closest)
			{
				closest   = gap;
				best_drop = drop;
			}

			if (drop >= deepest)
				break;

			drop = std::min(drop + EMBANKMENT_STEP, deepest);
		}

		const glm::vec3 base = top + direction * (best_drop * spread);
		return { base.x, sampleHeight(context, base.x, base.z) - EMBANKMENT_SINK, base.z };
	}

	void TerrainGenerator::buildDecorations(const RegionContext& context, lunar::World::ChunkCoord coord, const lunar::World::WorldSettings& settings, lunar::Render::MeshData& mesh) const
	{
		if (roads == nullptr)
			return;

		const glm::vec3                  origin     = lunar::World::ChunkOrigin(coord, settings);
		const glm::vec2                  minimum    = { origin.x, origin.z };
		const glm::vec2                  maximum    = minimum + glm::vec2(settings.getChunkSize());
		const RoadClass&                 road_class = roads->getRoadClass();
		const std::span<const glm::vec3> centreline = roads->getCentreline();
		const glm::vec3                  lift       = { 0.f, road_class.surfaceOffset, 0.f };

		for (const uint32_t segment : roads->segmentsWithin(minimum, maximum))
		{
			const glm::vec3 from_side  = roads->sideAt(segment);
			const glm::vec3 to_side    = roads->sideAt(segment + 1);
			const glm::vec3 from_left  = centreline[segment] + lift + from_side;
			const glm::vec3 from_right = centreline[segment] + lift - from_side;
			const glm::vec3 to_left    = centreline[segment + 1] + lift + to_side;
			const glm::vec3 to_right   = centreline[segment + 1] + lift - to_side;

			appendQuad(mesh, { from_left, from_right, to_left, to_right }, origin, road_class.color);
			appendQuad(mesh, { baseEdge(context, from_left, from_side), from_left, baseEdge(context, to_left, to_side), to_left }, origin, road_class.color);
			appendQuad(mesh, { from_right, baseEdge(context, from_right, -from_side), to_right, baseEdge(context, to_right, -to_side) }, origin, road_class.color);
		}
	}

	void TerrainGenerator::buildWater(const RegionContext& context, lunar::World::ChunkCoord coord, const lunar::World::WorldSettings& settings, lunar::Render::MeshData& mesh) const
	{
		const RegionPlan* plan = context.findRegion(lunar::World::RegionAt(coord, settings));
		if (plan == nullptr)
			return;

		const glm::vec3 origin  = lunar::World::ChunkOrigin(coord, settings);
		const glm::vec2 minimum = { origin.x, origin.z };
		const glm::vec2 maximum = minimum + glm::vec2(settings.getChunkSize());

		const float chunk_size = settings.getChunkSize();
		bool        submerged  = false;

		for (uint32_t z = 0; z <= OCEAN_PROBES && !submerged; z++)
			for (uint32_t x = 0; x <= OCEAN_PROBES && !submerged; x++)
				submerged = sampleHeight(context, origin.x + chunk_size * x / OCEAN_PROBES,
				                                  origin.z + chunk_size * z / OCEAN_PROBES) < elevation.seaLevel;

		if (submerged)
		{
			const float sea = elevation.seaLevel;

			appendQuad(mesh, { glm::vec3(origin.x, sea, origin.z),              glm::vec3(origin.x + chunk_size, sea, origin.z),
			                   glm::vec3(origin.x, sea, origin.z + chunk_size), glm::vec3(origin.x + chunk_size, sea, origin.z + chunk_size) },
			           origin, WATER_COLOR, WATER_ALPHA);
		}
	}

	void TerrainGenerator::refresh()
	{
		noises.clear();
		for (const Biome& biome : biomes->getBiomes())
			noises.push_back(MakeNoise(SeedFromHash(MixHash(static_cast<uint64_t>(seed), lunar::imp::fnv1a_hash(biome.name))), biome.terrain));
	}

	TerrainGenerator::~TerrainGenerator() noexcept = default;

	float TerrainGenerator::sampleHeight(const RegionContext& context, double x, double z) const
	{
		const Blend blend = gatherBlend(context, x, z);

		float height = elevationAt(x, z);
		for (size_t corner = 0; corner < BIOME_BLEND_CELLS; corner++)
			if (blend.weights[corner] > 0.f)
				height += blend.weights[corner] * biomeHeight(blend.indices[corner], x, z);

		const float carved = CarvedHeight(context, x, z, height);
		return roads == nullptr ? carved : roads->gradedHeight(x, z, carved);
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
