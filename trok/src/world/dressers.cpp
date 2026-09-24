#include <trok/world/dressers.hpp>

#include <lunar/physics/rigid_body.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr float    EMBANKMENT_SINK = 0.1f;
		constexpr float    EMBANKMENT_STEP = 1.f;
		constexpr float    WATER_ALPHA     = 0.55f;
		constexpr uint32_t OCEAN_PROBES    = 4;
		constexpr double   HALF            = 0.5;

		const glm::vec3    WATER_COLOR     = { 0.13f, 0.28f, 0.42f };

		void AppendQuad(lunar::Render::MeshData& mesh, const std::array<glm::vec3, QUAD_CORNERS>& corners, const glm::vec3& origin, const glm::vec3& color, float alpha = 1.f)
		{
			const auto      first  = static_cast<uint32_t>(mesh.vertices.size());
			const glm::vec3 facing = glm::normalize(glm::cross(corners[2] - corners[0], corners[1] - corners[0]));
			const glm::vec3 normal = facing.y < 0.f ? -facing : facing;

			for (const glm::vec3& corner : corners)
				mesh.vertices.push_back({ .position = corner - origin, .normal = normal, .color = glm::vec4(color, alpha) });

			for (const uint32_t index : { 0u, 2u, 1u, 1u, 2u, 3u })
				mesh.indices.push_back(first + index);
		}

		glm::vec3 BaseEdge(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& top, const glm::vec3& outward)
		{
			const glm::vec3 direction = outward / road_class.halfWidth();
			const float     spread    = road_class.embankmentSpread;
			const float     widest    = road_class.maxGradingWidth - road_class.halfWidth();
			const float     deepest   = std::max(spread > 0.f ? std::min(road_class.maxFillHeight, widest / spread) : road_class.maxFillHeight,
			                                     road_class.edgeDepth);

			float drop      = road_class.edgeDepth;
			float closest   = std::numeric_limits<float>::max();
			float best_drop = drop;

			while (true)
			{
				const glm::vec3 point = top + direction * (drop * spread);
				const float     gap   = top.y - drop - ground(point.x, point.z);

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
			return { base.x, ground(base.x, base.z) - EMBANKMENT_SINK, base.z };
		}
	}

	SeaDresser::SeaDresser(float sea_level) noexcept
		: seaLevel(sea_level)
	{
	}

	void SeaDresser::dress(const RegionContext&,
	                       lunar::World::ChunkCoord                coord,
	                       const lunar::World::WorldSettings&      settings,
	                       const lunar::World::HeightSampler&      ground,
	                       std::vector<lunar::World::DressedMesh>& output) const
	{
		const glm::vec3 origin     = lunar::World::ChunkOrigin(coord, settings);
		const float     chunk_size = settings.getChunkSize();
		bool            submerged  = false;

		for (uint32_t z = 0; z <= OCEAN_PROBES && !submerged; z++)
			for (uint32_t x = 0; x <= OCEAN_PROBES && !submerged; x++)
				submerged = ground(origin.x + chunk_size * x / OCEAN_PROBES, origin.z + chunk_size * z / OCEAN_PROBES) < seaLevel;

		if (!submerged)
			return;

		lunar::Render::MeshData mesh;
		AppendQuad(mesh, { glm::vec3(origin.x, seaLevel, origin.z),              glm::vec3(origin.x + chunk_size, seaLevel, origin.z),
		                   glm::vec3(origin.x, seaLevel, origin.z + chunk_size), glm::vec3(origin.x + chunk_size, seaLevel, origin.z + chunk_size) },
		           origin, WATER_COLOR, WATER_ALPHA);

		output.push_back({ .mesh = std::move(mesh), .translucent = true });
	}

	void RiverWaterDresser::dress(const RegionContext&                    context,
	                              lunar::World::ChunkCoord                coord,
	                              const lunar::World::WorldSettings&      settings,
	                              const lunar::World::HeightSampler&,
	                              std::vector<lunar::World::DressedMesh>& output) const
	{
		const RegionPlan* plan = context.findRegion(lunar::World::RegionAt(coord, settings));
		if (plan == nullptr)
			return;

		const glm::vec3 origin  = lunar::World::ChunkOrigin(coord, settings);
		const glm::vec2 minimum = { origin.x, origin.z };
		const glm::vec2 maximum = minimum + glm::vec2(settings.getChunkSize());

		lunar::Render::MeshData mesh;
		for (const River& river : plan->rivers)
		{
			for (size_t index = 0; index + 1 < river.points.size(); index++)
			{
				const RiverPoint& from   = river.points[index];
				const RiverPoint& to     = river.points[index + 1];
				const glm::vec2   middle = { (from.position.x + to.position.x) * HALF, (from.position.y + to.position.y) * HALF };
				if (middle.x < minimum.x || middle.x >= maximum.x || middle.y < minimum.y || middle.y >= maximum.y)
					continue;

				const glm::vec2 along = glm::normalize(glm::vec2(to.position - from.position));
				const glm::vec3 side  = { -along.y, 0.f, along.x };

				const glm::vec3 from_centre = { from.position.x, static_cast<float>(from.bed + RiverDepth(from.width)), from.position.y };
				const glm::vec3 to_centre   = { to.position.x,   static_cast<float>(to.bed   + RiverDepth(to.width)),   to.position.y };
				const glm::vec3 from_side   = side * static_cast<float>(from.width * HALF);
				const glm::vec3 to_side     = side * static_cast<float>(to.width * HALF);

				AppendQuad(mesh, { from_centre + from_side, from_centre - from_side, to_centre + to_side, to_centre - to_side },
				           origin, WATER_COLOR, WATER_ALPHA);
			}
		}

		output.push_back({ .mesh = std::move(mesh), .translucent = true });
	}

	RoadDresser::RoadDresser(std::shared_ptr<const RoadLayer> roads) noexcept
		: roads(std::move(roads))
	{
	}

	void RoadDresser::dress(const RegionContext&,
	                        lunar::World::ChunkCoord                coord,
	                        const lunar::World::WorldSettings&      settings,
	                        const lunar::World::HeightSampler&      ground,
	                        std::vector<lunar::World::DressedMesh>& output) const
	{
		const RoadNetwork* network = roads->get();
		if (network == nullptr)
			return;

		const glm::vec3                  origin     = lunar::World::ChunkOrigin(coord, settings);
		const glm::vec2                  minimum    = { origin.x, origin.z };
		const glm::vec2                  maximum    = minimum + glm::vec2(settings.getChunkSize());
		const RoadClass&                 road_class = network->getRoadClass();
		const std::span<const glm::vec3> centreline = network->getCentreline();
		const glm::vec3                  lift       = { 0.f, road_class.surfaceOffset, 0.f };

		lunar::Render::MeshData mesh;
		for (const uint32_t segment : network->segmentsWithin(minimum, maximum))
		{
			const glm::vec3 from_side  = network->sideAt(segment);
			const glm::vec3 to_side    = network->sideAt(segment + 1);
			const glm::vec3 from_left  = centreline[segment] + lift + from_side;
			const glm::vec3 from_right = centreline[segment] + lift - from_side;
			const glm::vec3 to_left    = centreline[segment + 1] + lift + to_side;
			const glm::vec3 to_right   = centreline[segment + 1] + lift - to_side;

			AppendQuad(mesh, { from_left, from_right, to_left, to_right }, origin, road_class.color);
			AppendQuad(mesh, { BaseEdge(ground, road_class, from_left, from_side), from_left, BaseEdge(ground, road_class, to_left, to_side), to_left }, origin, road_class.color);
			AppendQuad(mesh, { from_right, BaseEdge(ground, road_class, from_right, -from_side), to_right, BaseEdge(ground, road_class, to_right, -to_side) }, origin, road_class.color);
		}

		output.push_back({ .mesh = std::move(mesh), .colliderCategory = lunar::Physics::ROAD_CATEGORY });
	}
}
