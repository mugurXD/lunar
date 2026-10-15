#include <trok/world/dressers.hpp>

#include <lunar/physics/rigid_body.hpp>

#include <algorithm>
#include <cmath>
#include <array>
#include <limits>
#include <span>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr float    GROUND_SINK     = 0.1f;
		constexpr float    MARKING_LIFT    = 0.03f;
		constexpr float    LEFT            = 1.f;
		constexpr float    RIGHT           = -1.f;
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

		void AppendTriangle(lunar::Render::MeshData& mesh, glm::vec3 first, glm::vec3 second, glm::vec3 third, bool facing_up, const glm::vec3& origin, const glm::vec3& color)
		{
			glm::vec3 facing = glm::cross(second - first, third - first);
			if ((facing.y > 0.f) != facing_up)
			{
				std::swap(second, third);
				facing = -facing;
			}

			const glm::vec3 normal = glm::normalize(facing);
			for (const glm::vec3& corner : { first, second, third })
			{
				mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
				mesh.vertices.push_back({ .position = corner - origin, .normal = normal, .color = glm::vec4(color, 1.f) });
			}
		}

		void AppendDoubleSided(lunar::Render::MeshData& mesh, const std::array<glm::vec3, QUAD_CORNERS>& corners, const glm::vec3& origin, const glm::vec3& color)
		{
			AppendQuad(mesh, corners, origin, color);
			AppendQuad(mesh, { corners[1], corners[0], corners[3], corners[2] }, origin, color);
		}

		bool IsBridge(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& centre)
		{
			return centre.y - ground(centre.x, centre.z) > road_class.bridgeHeight;
		}

		glm::vec3 Outward(const glm::vec3& centre, const glm::vec3& edge)
		{
			return glm::normalize(glm::vec3(edge.x - centre.x, 0.f, edge.z - centre.z));
		}

		glm::vec3 Underside(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& top, const glm::vec3& outward, float sink, bool bridge)
		{
			const float deck = top.y - road_class.edgeDepth;
			if (bridge)
				return { top.x, deck, top.z };

			const float edge_ground = ground(top.x, top.z);
			const float drop        = std::max(top.y - edge_ground, 0.f);
			if (drop * road_class.edgeSpread > road_class.maxEdgeRun)
				return { top.x, std::min(edge_ground - GROUND_SINK, deck), top.z };

			const glm::vec3 foot = top + outward * ((drop + sink) * road_class.edgeSpread);
			return { foot.x, std::min(edge_ground, ground(foot.x, foot.z)) - sink, foot.z };
		}

		void AppendColumn(lunar::Render::MeshData& mesh,
		                  const glm::vec3&         top,
		                  float                    bottom,
		                  const glm::vec3&         along,
		                  float                    width,
		                  const glm::vec3&         origin,
		                  const glm::vec3&         color)
		{
			const glm::vec3 forward = along * (width * static_cast<float>(HALF));
			const glm::vec3 side    = glm::vec3(-forward.z, 0.f, forward.x);
			const glm::vec3 drop    = { 0.f, top.y - bottom, 0.f };

			const std::array<glm::vec3, QUAD_CORNERS> corners = { top + forward + side, top + forward - side, top - forward - side, top - forward + side };
			for (size_t corner = 0; corner < QUAD_CORNERS; corner++)
			{
				const glm::vec3& from = corners[corner];
				const glm::vec3& to   = corners[(corner + 1) % QUAD_CORNERS];
				AppendQuad(mesh, { from - drop, from, to - drop, to }, origin, color);
			}
		}

		template<typename Visit>
		void EveryInterval(float start, float end, float spacing, Visit visit)
		{
			for (float distance = std::ceil(start / spacing) * spacing; distance < end; distance += spacing)
				visit(distance);
		}

		struct SegmentFrame
		{
			glm::vec3 from     = {};
			glm::vec3 to       = {};
			glm::vec3 fromSide = {};
			glm::vec3 toSide   = {};
			float     start    = 0.f;
			float     end      = 0.f;
		};

		glm::vec3 SurfacePoint(const SegmentFrame& frame, float distance, float offset)
		{
			const float amount = frame.end > frame.start ? (distance - frame.start) / (frame.end - frame.start) : 0.f;
			return glm::mix(frame.from, frame.to, amount) + glm::mix(frame.fromSide, frame.toSide, amount) * offset;
		}

		void AppendStripe(lunar::Render::MeshData& mesh, const SegmentFrame& frame, float from, float to, float offset, float width, const glm::vec3& origin, const glm::vec3& color)
		{
			const glm::vec3 lift = { 0.f, MARKING_LIFT, 0.f };
			const float     half = width * static_cast<float>(HALF);

			AppendQuad(mesh, { SurfacePoint(frame, from, offset + half) + lift, SurfacePoint(frame, from, offset - half) + lift,
			                   SurfacePoint(frame, to,   offset + half) + lift, SurfacePoint(frame, to,   offset - half) + lift }, origin, color);
		}

		void AppendMarkings(lunar::Render::MeshData& mesh, const SegmentFrame& frame, const RoadClass& road_class, const glm::vec3& origin)
		{
			const RoadFurniture& furniture = road_class.furniture;
			const float          lanes     = static_cast<float>(road_class.lanes) * road_class.laneWidth * static_cast<float>(HALF);
			const float          period    = furniture.dashLength + furniture.dashGap;

			for (const float side : { LEFT, RIGHT })
				AppendStripe(mesh, frame, frame.start, frame.end, side * lanes, furniture.markingWidth, origin, furniture.markingColor);

			for (int32_t divider = 1; divider < road_class.lanes; divider++)
			{
				const float offset = static_cast<float>(divider) * road_class.laneWidth - lanes;
				if (divider * 2 == road_class.lanes)
				{
					AppendStripe(mesh, frame, frame.start, frame.end, offset, furniture.markingWidth, origin, furniture.centreColor);
					continue;
				}

				for (float dash = std::floor(frame.start / period) * period; dash < frame.end; dash += period)
				{
					const float from = std::max(dash, frame.start);
					const float to   = std::min(dash + furniture.dashLength, frame.end);
					if (from < to)
						AppendStripe(mesh, frame, from, to, offset, furniture.markingWidth, origin, furniture.markingColor);
				}
			}
		}

		void AppendWall(lunar::Render::MeshData& mesh, const std::array<glm::vec3, QUAD_CORNERS>& corners, bool faces_left, const glm::vec3& origin, const glm::vec3& color)
		{
			const auto& [from_bottom, from_top, to_bottom, to_top] = corners;
			AppendQuad(mesh, faces_left ? std::array { from_bottom, from_top, to_bottom, to_top } : std::array { from_top, from_bottom, to_top, to_bottom }, origin, color);
		}

		void AppendRail(lunar::Render::MeshData& mesh, const SegmentFrame& frame, const RoadClass& road_class, float side, const glm::vec3& origin)
		{
			const RoadFurniture& furniture = road_class.furniture;
			const float          outer     = side * road_class.halfWidth();
			const float          inner     = side * (road_class.halfWidth() - furniture.railThickness);
			const float          left      = std::max(outer, inner);
			const float          right     = std::min(outer, inner);
			const glm::vec3      top       = { 0.f, furniture.railHeight, 0.f };
			const glm::vec3      bottom    = { 0.f, furniture.railHeight - furniture.railDepth, 0.f };

			const auto edge = [&](float distance, float offset, const glm::vec3& height) {
				return SurfacePoint(frame, distance, offset) + height;
			};

			AppendWall(mesh, { edge(frame.start, left,  bottom), edge(frame.start, left,  top), edge(frame.end, left,  bottom), edge(frame.end, left,  top) }, true,  origin, furniture.railColor);
			AppendWall(mesh, { edge(frame.start, right, bottom), edge(frame.start, right, top), edge(frame.end, right, bottom), edge(frame.end, right, top) }, false, origin, furniture.railColor);
			AppendQuad(mesh, { edge(frame.start, left, top), edge(frame.start, right, top), edge(frame.end, left, top), edge(frame.end, right, top) }, origin, furniture.railColor);
			AppendQuad(mesh, { edge(frame.start, right, bottom), edge(frame.start, left, bottom), edge(frame.end, right, bottom), edge(frame.end, left, bottom) }, origin, furniture.railColor);
		}

		bool InsideOtherRoad(const RoadNetwork& network, const glm::vec3& point, size_t own_road, float clearance)
		{
			return network.closestPoint({ point.x, point.z }, network.getRoadClass().halfWidth() + clearance, own_road).has_value();
		}

		void AppendNode(lunar::Render::MeshData& mesh, const RoadNetwork& network, size_t node_index, const lunar::World::HeightSampler& ground, float sink, const glm::vec3& origin)
		{
			const RoadClass&              road_class = network.getRoadClass();
			const RoadNode&               node       = network.getNodes()[node_index];
			const std::vector<NodeCorner> outline    = network.nodeOutline(node_index);
			const glm::vec3               lift       = { 0.f, road_class.surfaceOffset, 0.f };
			const glm::vec3               depth      = { 0.f, road_class.edgeDepth, 0.f };
			const glm::vec3               centre     = node.position + lift;
			const bool                    bridge     = IsBridge(ground, road_class, node.position);

			for (size_t index = 0; index < outline.size(); index++)
			{
				const NodeCorner& from     = outline[index];
				const NodeCorner& to       = outline[(index + 1) % outline.size()];
				const glm::vec3   from_top = from.position + lift;
				const glm::vec3   to_top   = to.position + lift;

				AppendTriangle(mesh, centre, from_top, to_top, true, origin, road_class.color);
				if (bridge)
					AppendTriangle(mesh, centre - depth, from_top - depth, to_top - depth, false, origin, road_class.color);

				if (!from.opensRoad)
				{
					const glm::vec3 from_base = Underside(ground, road_class, from_top, Outward(node.position, from_top), sink, bridge);
					const glm::vec3 to_base   = Underside(ground, road_class, to_top, Outward(node.position, to_top), sink, bridge);
					AppendDoubleSided(mesh, { from_base, from_top, to_base, to_top }, origin, road_class.color);
				}
			}

			if (bridge)
				AppendColumn(mesh, centre - depth, ground(centre.x, centre.z) - GROUND_SINK, glm::vec3(1.f, 0.f, 0.f), road_class.pillarWidth, origin, road_class.color);
		}

		bool IsElevated(const lunar::World::HeightSampler& ground, const RoadClass& road_class, const glm::vec3& surface, const glm::vec3& side, float direction)
		{
			const glm::vec3 edge = surface + side * (direction * road_class.halfWidth());
			return edge.y - ground(edge.x, edge.z) > road_class.furniture.railElevation;
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
		const glm::vec3                  lift       = { 0.f, road_class.surfaceOffset, 0.f };
		const float                      ramp_sink  = std::max(GROUND_SINK, settings.maxFacetError);

		lunar::Render::MeshData mesh;
		lunar::Render::MeshData rails;
		lunar::Render::MeshData markings;
		lunar::Render::MeshData posts;
		for (const uint32_t segment : network->segmentsWithin(minimum, maximum))
		{
			const size_t road  = network->getRoadAt(segment);
			const float  start = std::max(network->distanceAt(segment), network->trimAt(road, true));
			const float  end   = std::min(network->distanceAt(segment + 1), network->lengthOf(road) - network->trimAt(road, false));
			if (start >= end)
				continue;

			const glm::vec3 from        = network->pointAlong(road, start);
			const glm::vec3 to          = network->pointAlong(road, end);
			const bool      from_bridge = IsBridge(ground, road_class, from);
			const bool      to_bridge   = IsBridge(ground, road_class, to);
			const glm::vec3 from_side   = network->sideAlong(road, start);
			const glm::vec3 to_side     = network->sideAlong(road, end);
			const glm::vec3 from_left   = from + lift + from_side;
			const glm::vec3 from_right  = from + lift - from_side;
			const glm::vec3 to_left     = to + lift + to_side;
			const glm::vec3 to_right    = to + lift - to_side;

			const glm::vec3 from_left_base  = Underside(ground, road_class, from_left, Outward(from, from_left), ramp_sink, from_bridge);
			const glm::vec3 from_right_base = Underside(ground, road_class, from_right, Outward(from, from_right), ramp_sink, from_bridge);
			const glm::vec3 to_left_base    = Underside(ground, road_class, to_left, Outward(to, to_left), ramp_sink, to_bridge);
			const glm::vec3 to_right_base   = Underside(ground, road_class, to_right, Outward(to, to_right), ramp_sink, to_bridge);

			AppendQuad(mesh, { from_left, from_right, to_left, to_right }, origin, road_class.color);
			AppendQuad(mesh, { from_left_base, from_left, to_left_base, to_left }, origin, road_class.color);
			AppendQuad(mesh, { from_right, from_right_base, to_right, to_right_base }, origin, road_class.color);

			const float        width = road_class.halfWidth();
			const SegmentFrame frame =
			{
				.from     = from + lift,
				.to       = to + lift,
				.fromSide = from_side / width,
				.toSide   = to_side / width,
				.start    = start,
				.end      = end
			};
			const glm::vec3 along = glm::normalize(glm::vec3(to.x - from.x, 0.f, to.z - from.z));


			const auto clear_of = [&](const glm::vec3& point, float clearance) { return !InsideOtherRoad(*network, point, road, clearance); };

			AppendMarkings(markings, frame, road_class, origin);

			for (const float side : { LEFT, RIGHT })
			{
				if (IsElevated(ground, road_class, frame.from, frame.fromSide, side) || IsElevated(ground, road_class, frame.to, frame.toSide, side))
				{
					const float rail_clearance = road_class.furniture.railThickness;
					if (clear_of(SurfacePoint(frame, frame.start, side * width), rail_clearance) && clear_of(SurfacePoint(frame, frame.end, side * width), rail_clearance))
						AppendRail(rails, frame, road_class, side, origin);

					continue;
				}

				EveryInterval(frame.start, frame.end, road_class.furniture.postSpacing, [&](float distance) {
					const glm::vec3 base = SurfacePoint(frame, distance, side * (width + road_class.furniture.postOutset));
					if (!clear_of(base, road_class.furniture.postWidth))
						return;

					const float foot = ground(base.x, base.z);
					AppendColumn(posts, { base.x, foot + road_class.furniture.postHeight, base.z }, foot - GROUND_SINK, along, road_class.furniture.postWidth, origin, road_class.furniture.postColor);
				});
			}

			if (!from_bridge && !to_bridge)
				continue;

			AppendQuad(mesh, { from_right_base, from_left_base, to_right_base, to_left_base }, origin, road_class.color);

			EveryInterval(frame.start, frame.end, road_class.pillarSpacing, [&](float distance) {
				const glm::vec3 centre = glm::mix(from, to, (distance - frame.start) / (frame.end - frame.start));
				if (IsBridge(ground, road_class, centre) && clear_of(centre, road_class.pillarWidth * static_cast<float>(HALF)))
					AppendColumn(mesh, centre + lift - glm::vec3(0.f, road_class.edgeDepth, 0.f), ground(centre.x, centre.z) - GROUND_SINK, along, road_class.pillarWidth, origin, road_class.color);
			});
		}

		for (const uint32_t node : network->nodesWithin(minimum, maximum))
			AppendNode(mesh, *network, node, ground, ramp_sink, origin);

		output.push_back({ .mesh = std::move(mesh), .colliderCategory = lunar::Physics::ROAD_CATEGORY });

		if (!rails.vertices.empty())
			output.push_back({ .mesh = std::move(rails), .colliderCategory = lunar::Physics::ROAD_CATEGORY });

		if (!markings.vertices.empty())
			output.push_back({ .mesh = std::move(markings) });

		if (!posts.vertices.empty())
			output.push_back({ .mesh = std::move(posts) });
	}
}
