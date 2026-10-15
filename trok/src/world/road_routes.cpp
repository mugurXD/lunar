#include <trok/world/road_routes.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr size_t DIRECTIONS         = 2;
		constexpr bool   RIGHT_HAND_TRAFFIC = true;
		constexpr float  HALF               = 0.5f;

		size_t StateOf(size_t road, bool forward)
		{
			return road * DIRECTIONS + (forward ? 1 : 0);
		}

		bool IsForward(size_t state)
		{
			return state % DIRECTIONS == 1;
		}

		size_t EndNodeOf(const RoadNetwork& network, size_t state)
		{
			return network.getNodeAt(state / DIRECTIONS, !IsForward(state));
		}

		std::vector<RouteStep> Unwind(const RoadNetwork& network, std::span<const std::optional<size_t>> previous, size_t last)
		{
			std::vector<RouteStep> route;
			for (std::optional<size_t> state = last; state.has_value(); state = previous[*state])
				route.push_back({ .road = network.getRoadId(*state / DIRECTIONS), .forward = IsForward(*state) });

			std::ranges::reverse(route);
			return route;
		}

		float FlatDistance(const glm::vec3& from, const glm::vec3& to)
		{
			return glm::distance(glm::vec2(from.x, from.z), glm::vec2(to.x, to.z));
		}
	}

	std::optional<std::vector<RouteStep>> FindRoute(const RoadNetwork& network, const RouteStep& from, size_t destination)
	{
		const std::optional<size_t> start_road = network.findRoad(from.road);
		if (!start_road.has_value())
			return std::nullopt;

		const float     speed  = network.getRoadClass().speedLimit;
		const glm::vec3 target = network.getNodes()[destination].position;

		const auto estimate = [&](size_t state) { return glm::distance(network.getNodes()[EndNodeOf(network, state)].position, target) / speed; };

		using Entry = std::pair<float, size_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
		std::vector<float>                                             costs(network.getRoadCount() * DIRECTIONS, std::numeric_limits<float>::infinity());
		std::vector<std::optional<size_t>>                             previous(costs.size());

		const size_t start = StateOf(*start_road, from.forward);
		costs[start] = 0.f;
		open.push({ estimate(start), start });

		while (!open.empty())
		{
			const auto [priority, state] = open.top();
			open.pop();

			if (EndNodeOf(network, state) == destination)
				return Unwind(network, previous, state);

			if (priority > costs[state] + estimate(state))
				continue;

			const size_t u_turn = StateOf(state / DIRECTIONS, !IsForward(state));
			for (const RoadEnd& end : network.getNodes()[EndNodeOf(network, state)].ends)
			{
				const size_t next = StateOf(end.road, end.atStart);
				const float  cost = costs[state] + network.lengthOf(end.road) / speed;
				if (next == u_turn || cost >= costs[next])
					continue;

				costs[next]    = cost;
				previous[next] = state;
				open.push({ cost + estimate(next), next });
			}
		}

		return std::nullopt;
	}

	std::optional<size_t> NearestNode(const RoadNetwork& network, const glm::vec3& position, float max_distance)
	{
		const std::span<const RoadNode> nodes       = network.getNodes();
		const auto                      distance_to = [&](const RoadNode& node) { return FlatDistance(node.position, position); };
		const auto                      nearest     = std::ranges::min_element(nodes, {}, distance_to);
		if (nearest == nodes.end() || distance_to(*nearest) > max_distance)
			return std::nullopt;

		return static_cast<size_t>(nearest - nodes.begin());
	}

	float LaneOffset(const RoadClass& road_class, bool forward)
	{
		const float centre = road_class.laneWidth * (static_cast<float>(road_class.lanes) * HALF - HALF);
		return forward == RIGHT_HAND_TRAFFIC ? centre : -centre;
	}

	glm::vec3 LanePoint(const RoadNetwork& network, size_t road, bool forward, float distance)
	{
		const RoadClass& road_class = network.getRoadClass();
		const float      lane       = LaneOffset(road_class, forward) / road_class.halfWidth();
		return network.pointAlong(road, distance) + network.sideAlong(road, distance) * lane + glm::vec3(0.f, road_class.surfaceOffset, 0.f);
	}

	std::vector<glm::vec3> RouteCheckpoints(const RoadNetwork& network, std::span<const RouteStep> route, float start_distance, float spacing)
	{
		std::vector<glm::vec3> checkpoints;
		for (size_t step = 0; step < route.size(); step++)
		{
			const std::optional<size_t> road = network.findRoad(route[step].road);
			if (!road.has_value())
				return {};

			const bool  forward = route[step].forward;
			const float head    = network.trimAt(*road, true);
			const float tail    = network.lengthOf(*road) - network.trimAt(*road, false);
			const float to      = forward ? tail : head;
			const float entry   = forward ? head : tail;
			const float from    = step == 0 ? (forward ? std::min(start_distance, to) : std::max(start_distance, to)) : entry;
			const int   pieces  = std::max(1, static_cast<int>(std::ceil(std::abs(to - from) / spacing)));

			for (int piece = 0; piece <= pieces; piece++)
				checkpoints.push_back(LanePoint(network, *road, forward, glm::mix(from, to, static_cast<float>(piece) / static_cast<float>(pieces))));
		}

		return checkpoints;
	}
}
