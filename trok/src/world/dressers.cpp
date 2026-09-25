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
		constexpr float    GROUND_SINK     = 0.1f;
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

		bool IsBridge(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& centre)
		{
			return centre.y - ground(centre.x, centre.z) > road_class.bridgeHeight;
		}

		glm::vec3 Underside(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& top, bool bridge)
		{
			const float deck = top.y - road_class.edgeDepth;
			return { top.x, bridge ? deck : std::min(ground(top.x, top.z) - GROUND_SINK, deck), top.z };
		}

		void AppendPillar(lunar::Render::MeshData&           mesh,
		                  const lunar::World::HeightSampler& ground,
		                  const RoadClass&                   road_class,
		                  const glm::vec3&                   centre,
		                  const glm::vec3&                   along,
		                  const glm::vec3&                   origin)
		{
			const glm::vec3 forward = along * (road_class.pillarWidth * static_cast<float>(HALF));
			const glm::vec3 side    = glm::vec3(-forward.z, 0.f, forward.x);
			const glm::vec3 drop    = { 0.f, centre.y - road_class.edgeDepth - ground(centre.x, centre.z) + GROUND_SINK, 0.f };
			const glm::vec3 top     = centre - glm::vec3(0.f, road_class.edgeDepth, 0.f);

			const std::array<glm::vec3, QUAD_CORNERS> corners = { top + forward + side, top + forward - side, top - forward - side, top - forward + side };
			for (size_t corner = 0; corner < QUAD_CORNERS; corner++)
			{
				const glm::vec3& from = corners[corner];
				const glm::vec3& to   = corners[(corner + 1) % QUAD_CORNERS];
				AppendQuad(mesh, { from - drop, from, to - drop, to }, origin, road_class.color);
			}
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
		const std::shared_ptr<const RoadNetwork> network = roads->get();
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
			const glm::vec3& from        = centreline[segment];
			const glm::vec3& to          = centreline[segment + 1];
			const bool       from_bridge = IsBridge(ground, road_class, from);
			const bool       to_bridge   = IsBridge(ground, road_class, to);
			const glm::vec3  from_side   = network->sideAt(segment);
			const glm::vec3  to_side     = network->sideAt(segment + 1);
			const glm::vec3  from_left   = from + lift + from_side;
			const glm::vec3  from_right  = from + lift - from_side;
			const glm::vec3  to_left     = to + lift + to_side;
			const glm::vec3  to_right    = to + lift - to_side;

			const glm::vec3 from_left_base  = Underside(ground, road_class, from_left, from_bridge);
			const glm::vec3 from_right_base = Underside(ground, road_class, from_right, from_bridge);
			const glm::vec3 to_left_base    = Underside(ground, road_class, to_left, to_bridge);
			const glm::vec3 to_right_base   = Underside(ground, road_class, to_right, to_bridge);

			AppendQuad(mesh, { from_left, from_right, to_left, to_right }, origin, road_class.color);
			AppendQuad(mesh, { from_left_base, from_left, to_left_base, to_left }, origin, road_class.color);
			AppendQuad(mesh, { from_right, from_right_base, to_right, to_right_base }, origin, road_class.color);

			if (!from_bridge && !to_bridge)
				continue;

			AppendQuad(mesh, { from_right_base, from_left_base, to_right_base, to_left_base }, origin, road_class.color);

			const float     start = network->distanceAt(segment);
			const float     end   = network->distanceAt(segment + 1);
			const glm::vec3 along = glm::normalize(glm::vec3(to.x - from.x, 0.f, to.z - from.z));

			for (float pillar = std::ceil(start / road_class.pillarSpacing) * road_class.pillarSpacing; pillar < end; pillar += road_class.pillarSpacing)
			{
				const glm::vec3 centre = glm::mix(from, to, (pillar - start) / (end - start));
				if (IsBridge(ground, road_class, centre))
					AppendPillar(mesh, ground, road_class, centre + lift, along, origin);
			}
		}

		output.push_back({ .mesh = std::move(mesh), .colliderCategory = lunar::Physics::ROAD_CATEGORY });
	}
}
