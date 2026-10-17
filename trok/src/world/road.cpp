#include <trok/world/road.hpp>
#include <trok/json_math.hpp>

#include <lunar/file/json_file.hpp>
#include <lunar/utils/geometry.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr uint32_t ROAD_FORMAT_VERSION = 6;
		constexpr float    CELL_SIZE           = 64.f;
		constexpr float    HALF                = 0.5f;
		constexpr float    MIN_MITRE_SCALE     = 0.5f;
		constexpr float    NODE_MERGE_DISTANCE = 0.05f;
		constexpr float    MAX_NODE_RADIUS     = 2.5f;
		constexpr float    MIN_NODE_GAP        = 0.01f;
		constexpr float    ARRIVAL_BLEND       = 30.f;
		constexpr float    SMOOTHSTEP_PEAK     = 1.5f;
		constexpr float    STUB_MARGIN         = 6.f;
		constexpr float    STUB_SPACING        = 6.f;
		constexpr float    CORNER_MERGE        = 0.01f;
		constexpr float    KMH_PER_MPS         = 3.6f;

		float AngleOf(const glm::vec2& direction)
		{
			return std::atan2(direction.y, direction.x);
		}

		float GapBetween(float from, float to)
		{
			const float gap = to - from;
			return gap > 0.f ? gap : gap + glm::two_pi<float>();
		}

		float MeetingDistance(float gap, float width)
		{
			if (gap >= glm::pi<float>() - MIN_NODE_GAP)
				return 0.f;

			return std::min(width / std::tan(std::max(gap, MIN_NODE_GAP) * HALF), width * MAX_NODE_RADIUS);
		}

		std::vector<float> TrimsFor(std::span<const glm::vec2> directions, float width)
		{
			std::vector<size_t> order(directions.size());
			std::iota(order.begin(), order.end(), size_t(0));
			std::ranges::sort(order, {}, [&](size_t index) { return AngleOf(directions[index]); });

			std::vector<float> trims(directions.size(), width);
			for (size_t index = 0; index < order.size(); index++)
			{
				const size_t current = order[index];
				const size_t next    = order[(index + 1) % order.size()];
				const float  meeting = MeetingDistance(GapBetween(AngleOf(directions[current]), AngleOf(directions[next])), width);

				trims[current] = std::max(trims[current], meeting);
				trims[next]    = std::max(trims[next], meeting);
			}

			return trims;
		}

		glm::vec3 PointBeforeEnd(std::span<const glm::vec3> line, float distance)
		{
			float travelled = 0.f;
			for (size_t point = line.size() - 1; point > 0; point--)
			{
				const float step = glm::distance(line[point], line[point - 1]);
				if (travelled + step >= distance)
					return glm::mix(line[point], line[point - 1], (distance - travelled) / step);

				travelled += step;
			}

			return line.front();
		}

		float BlendLength(float rise, const RoadClass& road_class)
		{
			return std::max(ARRIVAL_BLEND, SMOOTHSTEP_PEAK * std::abs(rise) / road_class.maxGrade);
		}

		void BlendTowards(std::vector<glm::vec3>& line, float rise, float length)
		{
			float travelled = 0.f;
			for (size_t point = line.size() - 1; point-- > 0;)
			{
				travelled += glm::distance(line[point], line[point + 1]);
				if (travelled >= length)
					break;

				line[point].y += rise * (1.f - glm::smoothstep(0.f, length, travelled));
			}
		}

		void LevelEnd(std::vector<glm::vec3>& line, const glm::vec3& node, float stub, const RoadClass& road_class)
		{
			std::optional<glm::vec3> anchor;
			float                    travelled = 0.f;
			while (line.size() > 1 && !anchor.has_value())
			{
				const glm::vec3 inner = line.back();
				line.pop_back();

				const float step = glm::distance(inner, line.back());
				if (travelled + step >= stub)
					anchor = glm::mix(inner, line.back(), (stub - travelled) / step);

				travelled += step;
			}

			if (!anchor.has_value())
			{
				anchor = line.front();
				line.clear();
			}

			const float rise = node.y - anchor->y;
			line.push_back(*anchor);
			BlendTowards(line, rise, BlendLength(rise, road_class));
			line.back().y = node.y;

			const glm::vec3 level  = line.back();
			const int       pieces = std::max(1, static_cast<int>(std::ceil(glm::distance(level, node) / STUB_SPACING)));
			for (int piece = 1; piece < pieces; piece++)
				line.push_back(glm::mix(level, node, static_cast<float>(piece) / static_cast<float>(pieces)));

			line.push_back(node);
		}

		std::vector<RoadNode> FindNodes(const std::vector<std::vector<glm::vec3>>& roads)
		{
			std::vector<RoadNode> nodes;
			for (uint32_t road = 0; road < roads.size(); road++)
			{
				for (const bool at_start : { true, false })
				{
					const glm::vec3& end   = at_start ? roads[road].front() : roads[road].back();
					const auto       found = std::ranges::find_if(nodes, [&](const RoadNode& node) { return glm::distance(node.position, end) <= NODE_MERGE_DISTANCE; });
					if (found != nodes.end())
						found->ends.push_back({ .road = road, .atStart = at_start });
					else
						nodes.push_back({ .position = end, .ends = { { .road = road, .atStart = at_start } } });
				}
			}

			return nodes;
		}

		std::vector<std::vector<glm::vec3>> LevelJunctions(std::vector<std::vector<glm::vec3>> roads, const RoadClass& road_class)
		{
			const float width = road_class.halfWidth();

			for (const RoadNode& node : FindNodes(roads))
			{
				if (node.ends.size() < 2)
					continue;

				const auto toward_node = [&](const RoadEnd& end) {
					std::vector<glm::vec3> line = roads[end.road];
					if (end.atStart)
						std::ranges::reverse(line);

					return line;
				};

				std::vector<glm::vec2> directions;
				for (const RoadEnd& end : node.ends)
				{
					const glm::vec3 probe = PointBeforeEnd(toward_node(end), width);
					directions.push_back(glm::normalize(glm::vec2(probe.x - node.position.x, probe.z - node.position.z)));
				}

				const std::vector<float> trims = TrimsFor(directions, width);
				for (size_t index = 0; index < node.ends.size(); index++)
				{
					const RoadEnd&         end  = node.ends[index];
					std::vector<glm::vec3> line = toward_node(end);
					LevelEnd(line, node.position, trims[index] + STUB_MARGIN, road_class);
					if (end.atStart)
						std::ranges::reverse(line);

					roads[end.road] = std::move(line);
				}
			}

			return roads;
		}

		lunar::World::GridCoord<struct RoadCellTag> CellAt(float x, float z)
		{
			return { static_cast<int32_t>(std::floor(x / CELL_SIZE)), static_cast<int32_t>(std::floor(z / CELL_SIZE)) };
		}

		glm::vec3 Perpendicular(const glm::vec3& from, const glm::vec3& to)
		{
			const glm::vec2 direction = glm::normalize(glm::vec2(to.x - from.x, to.z - from.z));
			return { -direction.y, 0.f, direction.x };
		}

		float FlatWidth(const RoadClass& road_class)
		{
			return road_class.halfWidth() + road_class.gradingMargin;
		}

		float GradingReach(const RoadClass& road_class)
		{
			return FlatWidth(road_class) + road_class.maxCutDepth * road_class.cutSpread;
		}

		bool Reaches(const glm::vec3& from, const glm::vec3& to, const glm::vec2& minimum, const glm::vec2& maximum, float reach)
		{
			return std::max(from.x, to.x) + reach >= minimum.x && std::min(from.x, to.x) - reach <= maximum.x &&
			       std::max(from.z, to.z) + reach >= minimum.y && std::min(from.z, to.z) - reach <= maximum.y;
		}

		nlohmann::json SerializeFurniture(const RoadFurniture& furniture)
		{
			return nlohmann::json
			{
				{ "markingWidth",  furniture.markingWidth },
				{ "dashLength",    furniture.dashLength },
				{ "dashGap",       furniture.dashGap },
				{ "markingColor",  SerializeVec3(furniture.markingColor) },
				{ "centreColor",   SerializeVec3(furniture.centreColor) },
				{ "postSpacing",   furniture.postSpacing },
				{ "postHeight",    furniture.postHeight },
				{ "postWidth",     furniture.postWidth },
				{ "postOutset",    furniture.postOutset },
				{ "postColor",     SerializeVec3(furniture.postColor) },
				{ "railElevation", furniture.railElevation },
				{ "railHeight",    furniture.railHeight },
				{ "railDepth",     furniture.railDepth },
				{ "railThickness", furniture.railThickness },
				{ "railColor",     SerializeVec3(furniture.railColor) }
			};
		}

		RoadFurniture DeserializeFurniture(const nlohmann::json& json)
		{
			return RoadFurniture
			{
				.markingWidth  = json.at("markingWidth").get<float>(),
				.dashLength    = json.at("dashLength").get<float>(),
				.dashGap       = json.at("dashGap").get<float>(),
				.markingColor  = DeserializeVec3(json.at("markingColor")),
				.centreColor   = DeserializeVec3(json.at("centreColor")),
				.postSpacing   = json.at("postSpacing").get<float>(),
				.postHeight    = json.at("postHeight").get<float>(),
				.postWidth     = json.at("postWidth").get<float>(),
				.postOutset    = json.at("postOutset").get<float>(),
				.postColor     = DeserializeVec3(json.at("postColor")),
				.railElevation = json.at("railElevation").get<float>(),
				.railHeight    = json.at("railHeight").get<float>(),
				.railDepth     = json.at("railDepth").get<float>(),
				.railThickness = json.at("railThickness").get<float>(),
				.railColor     = DeserializeVec3(json.at("railColor"))
			};
		}

		lunar::World::ShapePoint ShapePointOf(const glm::vec3& point, float core)
		{
			return { .position = { point.x, point.z }, .height = point.y, .core = core };
		}
	}

	RoadConnectivity ConnectRoads(const RoadNetwork& network, size_t town_count, const TownLocator& town_at)
	{
		const size_t        road_count = network.getRoadCount();
		std::vector<size_t> parents(road_count + town_count);
		std::vector<bool>   reached(town_count, false);
		std::iota(parents.begin(), parents.end(), size_t(0));

		const auto root = [&](size_t node) {
			while (parents[node] != node)
				node = parents[node] = parents[parents[node]];

			return node;
		};

		const auto unite = [&](size_t first, size_t second) { parents[root(first)] = root(second); };

		for (size_t road = 0; road < road_count; road++)
		{
			const std::span<const glm::vec3> points = network.getRoad(road);
			for (const glm::vec3& end : { points.front(), points.back() })
			{
				const std::optional<size_t> town = town_at(end);
				if (!town.has_value())
					continue;

				unite(road, road_count + *town);
				reached[*town] = true;
			}
		}

		for (const RoadNode& node : network.getNodes())
			for (const RoadEnd& end : node.ends)
				unite(end.road, node.ends.front().road);

		RoadConnectivity connectivity = { .roadGroups = std::vector<size_t>(road_count), .townGroups = std::vector<std::optional<size_t>>(town_count) };
		for (size_t road = 0; road < road_count; road++)
			connectivity.roadGroups[road] = root(road);

		for (size_t town = 0; town < town_count; town++)
			if (reached[town])
				connectivity.townGroups[town] = root(road_count + town);

		return connectivity;
	}

	std::vector<lunar::World::ChunkCoord> ChunksReachedBy(std::span<const glm::vec3> road, const RoadClass& road_class, const lunar::World::WorldSettings& settings)
	{
		const float stub  = road_class.halfWidth() * MAX_NODE_RADIUS + STUB_MARGIN;
		const float reach = GradingReach(road_class) + stub + std::max(ARRIVAL_BLEND, SMOOTHSTEP_PEAK * stub) + settings.vertexSpacing * lunar::World::HEIGHTMAP_BORDER;

		std::unordered_set<lunar::World::ChunkCoord, lunar::World::GridCoordHash> reached;
		for (size_t point = 1; point < road.size(); point++)
		{
			const glm::vec3&               from    = road[point - 1];
			const glm::vec3&               to      = road[point];
			const lunar::World::ChunkCoord minimum = lunar::World::ChunkAt(std::min(from.x, to.x) - reach, std::min(from.z, to.z) - reach, settings);
			const lunar::World::ChunkCoord maximum = lunar::World::ChunkAt(std::max(from.x, to.x) + reach, std::max(from.z, to.z) + reach, settings);

			for (int32_t z = minimum.z; z <= maximum.z; z++)
				for (int32_t x = minimum.x; x <= maximum.x; x++)
					reached.insert({ x, z });
		}

		return { reached.begin(), reached.end() };
	}

	float RoadClass::halfWidth() const
	{
		return static_cast<float>(lanes) * laneWidth * HALF + shoulderWidth;
	}

	float RoadClass::speedLimitMetresPerSecond() const
	{
		return speedLimit / KMH_PER_MPS;
	}

	nlohmann::json RoadClass::Serialize(const RoadClass& road_class)
	{
		return nlohmann::json
		{
			{ "formatVersion",   ROAD_FORMAT_VERSION },
			{ "name",            road_class.name },
			{ "lanes",           road_class.lanes },
			{ "laneWidth",       road_class.laneWidth },
			{ "shoulderWidth",   road_class.shoulderWidth },
			{ "minCurveRadius",  road_class.minCurveRadius },
			{ "maxGrade",        road_class.maxGrade },
			{ "speedLimit",      road_class.speedLimit },
			{ "limitPenalty",    road_class.limitPenalty },
			{ "turnPenalty",     road_class.turnPenalty },
			{ "climbCost",       road_class.climbCost },
			{ "cutCost",         road_class.cutCost },
			{ "fillCost",        road_class.fillCost },
			{ "bridgeCost",      road_class.bridgeCost },
			{ "bridgeHeight",    road_class.bridgeHeight },
			{ "maxCutDepth",     road_class.maxCutDepth },
			{ "cutSpread",       road_class.cutSpread },
			{ "gradingMargin",   road_class.gradingMargin },
			{ "pillarSpacing",   road_class.pillarSpacing },
			{ "pillarWidth",     road_class.pillarWidth },
			{ "surfaceOffset",   road_class.surfaceOffset },
			{ "edgeDepth",       road_class.edgeDepth },
			{ "edgeSpread",      road_class.edgeSpread },
			{ "maxEdgeRun",      road_class.maxEdgeRun },
			{ "color",           SerializeVec3(road_class.color) },
			{ "furniture",       SerializeFurniture(road_class.furniture) }
		};
	}

	std::optional<RoadClass> RoadClass::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != ROAD_FORMAT_VERSION)
			return std::nullopt;

		const RoadClass road_class =
		{
			.name            = json.at("name").get<std::string>(),
			.lanes           = json.at("lanes").get<int32_t>(),
			.laneWidth       = json.at("laneWidth").get<float>(),
			.shoulderWidth   = json.at("shoulderWidth").get<float>(),
			.minCurveRadius  = json.at("minCurveRadius").get<float>(),
			.maxGrade        = json.at("maxGrade").get<float>(),
			.speedLimit      = json.at("speedLimit").get<float>(),
			.limitPenalty    = json.at("limitPenalty").get<float>(),
			.turnPenalty     = json.at("turnPenalty").get<float>(),
			.climbCost       = json.at("climbCost").get<float>(),
			.cutCost         = json.at("cutCost").get<float>(),
			.fillCost        = json.at("fillCost").get<float>(),
			.bridgeCost      = json.at("bridgeCost").get<float>(),
			.bridgeHeight    = json.at("bridgeHeight").get<float>(),
			.maxCutDepth     = json.at("maxCutDepth").get<float>(),
			.cutSpread       = json.at("cutSpread").get<float>(),
			.gradingMargin   = json.at("gradingMargin").get<float>(),
			.pillarSpacing   = json.at("pillarSpacing").get<float>(),
			.pillarWidth     = json.at("pillarWidth").get<float>(),
			.surfaceOffset   = json.at("surfaceOffset").get<float>(),
			.edgeDepth       = json.at("edgeDepth").get<float>(),
			.edgeSpread      = json.at("edgeSpread").get<float>(),
			.maxEdgeRun      = json.at("maxEdgeRun").get<float>(),
			.color           = DeserializeVec3(json.at("color")),
			.furniture       = DeserializeFurniture(json.at("furniture"))
		};

		if (road_class.lanes <= 0 || road_class.laneWidth <= 0.f || road_class.maxGrade <= 0.f || road_class.minCurveRadius <= 0.f || road_class.speedLimit <= 0.f ||
		    road_class.pillarSpacing <= 0.f || road_class.maxCutDepth <= 0.f || road_class.cutSpread < 0.f || road_class.edgeSpread < 0.f || road_class.maxEdgeRun < 0.f ||
		    road_class.furniture.dashLength <= 0.f || road_class.furniture.dashGap < 0.f || road_class.furniture.postSpacing <= 0.f)
		{
			Fs::ReportMalformedJson("a road class needs positive lanes, lane width, grade, curve radius, speed limit, cut depth, pillar, post and dash spacing, and a non-negative cut spread, edge spread, edge run and dash gap");
			return std::nullopt;
		}

		return road_class;
	}

	RoadNetwork::RoadNetwork(std::vector<glm::vec3> centreline, RoadClass road_class) noexcept
		: RoadNetwork(std::vector<std::vector<glm::vec3>> { std::move(centreline) }, std::move(road_class))
	{
	}

	RoadNetwork::RoadNetwork(std::vector<std::vector<glm::vec3>> centrelines, RoadClass road_class, std::vector<RoadId> ids) noexcept
		: roadIds(std::move(ids)),
		roadClass(std::move(road_class))
	{
		if (roadIds.empty())
		{
			roadIds.resize(centrelines.size());
			std::iota(roadIds.begin(), roadIds.end(), RoadId(0));
		}

		for (uint32_t road = 0; road < roadIds.size(); road++)
			roadIndices[roadIds[road]] = road;

		centrelines = LevelJunctions(std::move(centrelines), roadClass);

		for (const std::vector<glm::vec3>& road : centrelines)
		{
			roadStarts.push_back(static_cast<uint32_t>(points.size()));
			for (const glm::vec3& point : road)
			{
				distances.push_back(points.size() == roadStarts.back() ? 0.f : distances.back() + glm::distance(points.back(), point));
				roadOf.push_back(static_cast<uint32_t>(roadStarts.size() - 1));
				points.push_back(point);
			}
		}

		roadStarts.push_back(static_cast<uint32_t>(points.size()));

		const float reach = GradingReach(roadClass);

		for (uint32_t segment = 0; segment + 1 < points.size(); segment++)
		{
			if (isRoadEnd(segment))
				continue;

			const glm::vec3& from    = points[segment];
			const glm::vec3& to      = points[segment + 1];
			const Cell       minimum = CellAt(std::min(from.x, to.x) - reach, std::min(from.z, to.z) - reach);
			const Cell       maximum = CellAt(std::max(from.x, to.x) + reach, std::max(from.z, to.z) + reach);

			for (int32_t z = minimum.z; z <= maximum.z; z++)
				for (int32_t x = minimum.x; x <= maximum.x; x++)
					cells[{ x, z }].push_back(segment);
		}

		nodes = FindNodes(centrelines);
		roadNodes.resize(centrelines.size());
		for (uint32_t node = 0; node < nodes.size(); node++)
			for (const RoadEnd& end : nodes[node].ends)
				roadNodes[end.road][end.atStart ? 0 : 1] = node;

		for (RoadNode& node : nodes)
			computeTrims(node);
	}

	glm::vec2 RoadNetwork::directionOf(const RoadNode& node, const RoadEnd& end) const
	{
		const float     length = lengthOf(end.road);
		const float     probe  = std::min(roadClass.halfWidth(), length);
		const glm::vec3 toward = pointAlong(end.road, end.atStart ? probe : length - probe);
		return glm::normalize(glm::vec2(toward.x - node.position.x, toward.z - node.position.z));
	}

	void RoadNetwork::computeTrims(RoadNode& node) const
	{
		if (node.ends.size() < 2)
			return;

		std::ranges::sort(node.ends, {}, [&](const RoadEnd& end) { return AngleOf(directionOf(node, end)); });

		std::vector<glm::vec2> directions;
		for (const RoadEnd& end : node.ends)
			directions.push_back(directionOf(node, end));

		const std::vector<float> trims = TrimsFor(directions, roadClass.halfWidth());
		for (size_t index = 0; index < node.ends.size(); index++)
			node.ends[index].trim = trims[index];

		node.radius = std::ranges::max(node.ends, {}, &RoadEnd::trim).trim;
	}

	std::vector<NodeCorner> RoadNetwork::nodeOutline(size_t index) const
	{
		const RoadNode& node   = nodes[index];
		const float     width  = roadClass.halfWidth();
		const glm::vec2 centre = { node.position.x, node.position.z };

		std::vector<NodeCorner> outline;
		for (size_t end_index = 0; end_index < node.ends.size(); end_index++)
		{
			const RoadEnd&  end      = node.ends[end_index];
			const RoadEnd&  next     = node.ends[(end_index + 1) % node.ends.size()];
			const glm::vec2 forward  = directionOf(node, end);
			const float     distance = end.atStart ? end.trim : lengthOf(end.road) - end.trim;
			const glm::vec3 cut      = pointAlong(end.road, distance);
			const glm::vec3 side     = sideAlong(end.road, distance);
			const bool      ccw_side = end.atStart;

			outline.push_back({ ccw_side ? cut - side : cut + side, true });
			outline.push_back({ ccw_side ? cut + side : cut - side, false });

			const float meeting = MeetingDistance(GapBetween(AngleOf(forward), AngleOf(directionOf(node, next))), width);
			if (meeting > 0.f)
			{
				const glm::vec2 corner = centre + forward * meeting + glm::vec2(-forward.y, forward.x) * width;
				outline.push_back({ { corner.x, node.position.y, corner.y }, false });
			}
		}

		std::vector<NodeCorner> merged;
		for (size_t corner = 0; corner < outline.size(); corner++)
			if (glm::distance(outline[corner].position, outline[(corner + 1) % outline.size()].position) > CORNER_MERGE)
				merged.push_back(outline[corner]);

		return merged;
	}

	bool RoadNetwork::isRoadStart(size_t point) const
	{
		return point == roadStarts[roadOf[point]];
	}

	bool RoadNetwork::isRoadEnd(size_t point) const
	{
		return point + 1 == roadStarts[roadOf[point] + 1];
	}

	std::span<const glm::vec3> RoadNetwork::getRoad(size_t road) const
	{
		return std::span(points).subspan(roadStarts[road], roadStarts[road + 1] - roadStarts[road]);
	}

	size_t RoadNetwork::getRoadCount() const
	{
		return roadStarts.empty() ? 0 : roadStarts.size() - 1;
	}

	size_t RoadNetwork::getRoadAt(size_t point) const
	{
		return roadOf[point];
	}

	RoadId RoadNetwork::getRoadId(size_t road) const
	{
		return roadIds[road];
	}

	std::optional<size_t> RoadNetwork::findRoad(RoadId id) const
	{
		const auto found = roadIndices.find(id);
		return found != roadIndices.end() ? std::optional<size_t>(found->second) : std::nullopt;
	}

	size_t RoadNetwork::getNodeAt(size_t road, bool at_start) const
	{
		return roadNodes[road][at_start ? 0 : 1];
	}

	float RoadNetwork::lengthOf(size_t road) const
	{
		return distances[roadStarts[road + 1] - 1];
	}

	std::pair<size_t, float> RoadNetwork::locate(size_t road, float distance) const
	{
		const size_t first = roadStarts[road];
		const size_t last  = roadStarts[road + 1] - 1;
		if (distance <= 0.f)
			return { first, 0.f };

		if (distance >= distances[last])
			return { last - 1, 1.f };

		const auto   next     = std::upper_bound(distances.begin() + static_cast<std::ptrdiff_t>(first), distances.begin() + static_cast<std::ptrdiff_t>(last) + 1, distance);
		const size_t previous = static_cast<size_t>(next - distances.begin()) - 1;
		return { previous, (distance - distances[previous]) / (distances[previous + 1] - distances[previous]) };
	}

	glm::vec3 RoadNetwork::pointAlong(size_t road, float distance) const
	{
		const auto [previous, amount] = locate(road, distance);
		return amount >= 1.f ? points[previous + 1] : glm::mix(points[previous], points[previous + 1], amount);
	}

	glm::vec3 RoadNetwork::sideAlong(size_t road, float distance) const
	{
		const auto [previous, amount] = locate(road, distance);
		return amount >= 1.f ? sideAt(previous + 1) : glm::mix(sideAt(previous), sideAt(previous + 1), amount);
	}

	float RoadNetwork::trimAt(size_t road, bool at_start) const
	{
		const RoadNode& node = nodes[roadNodes[road][at_start ? 0 : 1]];
		if (node.ends.size() < 2)
			return 0.f;

		return std::ranges::find_if(node.ends, [&](const RoadEnd& end) { return end.road == road && end.atStart == at_start; })->trim;
	}

	std::span<const RoadNode> RoadNetwork::getNodes() const
	{
		return nodes;
	}

	std::vector<uint32_t> RoadNetwork::nodesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		std::vector<uint32_t> found;
		for (uint32_t node = 0; node < nodes.size(); node++)
		{
			const glm::vec3& position = nodes[node].position;
			if (nodes[node].ends.size() > 1 && position.x >= minimum.x && position.x < maximum.x && position.z >= minimum.y && position.z < maximum.y)
				found.push_back(node);
		}

		return found;
	}

	std::vector<glm::vec3> ArriveAt(std::vector<glm::vec3> road, const glm::vec3& node, const RoadClass& road_class)
	{
		const float rise = node.y - road.back().y;
		road.back()      = node;
		BlendTowards(road, rise, BlendLength(rise, road_class));
		return road;
	}

	std::vector<lunar::World::ShapeDeclaration> RoadNetwork::shapesReaching(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		const float core  = FlatWidth(roadClass);
		const float reach = GradingReach(roadClass);

		std::vector<lunar::World::ShapeDeclaration> shapes;
		uint32_t                                    previous = 0;

		for (const uint32_t segment : candidatesWithin(minimum, maximum))
		{
			const glm::vec3& from = points[segment];
			const glm::vec3& to   = points[segment + 1];
			if (!Reaches(from, to, minimum, maximum, reach))
				continue;

			const bool continues = !shapes.empty() && segment == previous + 1;
			if (!continues)
				shapes.push_back({ .points = { ShapePointOf(from, core) }, .spread = roadClass.cutSpread, .reach = reach });

			shapes.back().points.push_back(ShapePointOf(to, core));
			previous = segment;
		}

		for (const RoadNode& node : nodes)
		{
			const float node_core  = std::hypot(node.radius, roadClass.halfWidth()) + roadClass.gradingMargin;
			const float node_reach = node_core + roadClass.maxCutDepth * roadClass.cutSpread;
			if (node.ends.size() > 1 && Reaches(node.position, node.position, minimum, maximum, node_reach))
				shapes.push_back({ .points = { ShapePointOf(node.position, node_core), ShapePointOf(node.position, node_core) }, .spread = roadClass.cutSpread, .reach = node_reach });
		}

		return shapes;
	}

	glm::vec3 RoadNetwork::sideAt(size_t point) const
	{
		const bool      first  = isRoadStart(point);
		const bool      last   = isRoadEnd(point);
		const glm::vec3 before = Perpendicular(points[first ? point : point - 1], points[first ? point + 1 : point]);
		const glm::vec3 after  = Perpendicular(points[last ? point - 1 : point], points[last ? point : point + 1]);
		const glm::vec3 mitre  = glm::normalize(before + after);

		return mitre * (roadClass.halfWidth() / std::max(glm::dot(mitre, after), MIN_MITRE_SCALE));
	}

	float RoadNetwork::distanceAt(size_t point) const
	{
		return distances[point];
	}

	std::vector<uint32_t> RoadNetwork::segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		std::vector<uint32_t> found = candidatesWithin(minimum, maximum);

		std::erase_if(found, [&](uint32_t segment) {
			const glm::vec3 middle = (points[segment] + points[segment + 1]) * HALF;
			return middle.x < minimum.x || middle.x >= maximum.x || middle.z < minimum.y || middle.z >= maximum.y;
		});

		return found;
	}

	std::optional<RoadPoint> RoadNetwork::closestPoint(const glm::vec2& position, float max_distance, std::optional<size_t> excluded_road) const
	{
		std::optional<RoadPoint> closest;
		float                    shortest = max_distance;

		for (const uint32_t segment : candidatesWithin(position - glm::vec2(max_distance), position + glm::vec2(max_distance)))
		{
			const glm::vec3& from   = points[segment];
			const glm::vec3& to     = points[segment + 1];
			const glm::vec2  course = glm::vec2(to.x - from.x, to.z - from.z);
			if (course == glm::vec2(0.f) || roadOf[segment] == excluded_road)
				continue;

			float       amount   = 0.f;
			const float distance = lunar::DistanceToSegment(position, { from.x, from.z }, { to.x, to.z }, amount);
			if (distance > shortest)
				continue;

			shortest = distance;
			closest  = RoadPoint
			{
				.position  = glm::mix(from, to, amount),
				.direction = glm::normalize(course),
				.road      = roadOf[segment],
				.distance  = glm::mix(distances[segment], distances[segment + 1], amount)
			};
		}

		return closest;
	}

	std::vector<uint32_t> RoadNetwork::candidatesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		const Cell lowest  = CellAt(minimum.x, minimum.y);
		const Cell highest = CellAt(maximum.x, maximum.y);

		std::vector<uint32_t> found;

		for (int32_t z = lowest.z; z <= highest.z; z++)
		{
			for (int32_t x = lowest.x; x <= highest.x; x++)
			{
				const auto cell = cells.find({ x, z });
				if (cell == cells.end())
					continue;

				found.insert(found.end(), cell->second.begin(), cell->second.end());
			}
		}

		std::ranges::sort(found);
		const auto duplicates = std::ranges::unique(found);
		found.erase(duplicates.begin(), duplicates.end());
		return found;
	}

	std::span<const glm::vec3> RoadNetwork::getCentreline() const
	{
		return points;
	}

	const RoadClass& RoadNetwork::getRoadClass() const
	{
		return roadClass;
	}

	void RoadLayer::set(std::shared_ptr<const RoadNetwork> replacement)
	{
		const std::lock_guard lock(mutex);
		network = std::move(replacement);
	}

	std::shared_ptr<const RoadNetwork> RoadLayer::get() const
	{
		const std::lock_guard lock(mutex);
		return network;
	}
}
