#pragma once
#include <trok/world/road.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace trok
{
	struct RouteStep
	{
		RoadId road    = 0;
		bool   forward = true;

		bool operator==(const RouteStep&) const = default;
	};

	std::optional<std::vector<RouteStep>> FindRoute(const RoadNetwork& network, const RouteStep& from, size_t destination);
	std::optional<size_t>                 NearestNode(const RoadNetwork& network, const glm::vec3& position, float max_distance);
	float                                 LaneOffset(const RoadClass& road_class, bool forward);
	glm::vec3                             LanePoint(const RoadNetwork& network, size_t road, bool forward, float distance);
	std::vector<glm::vec3>                RouteCheckpoints(const RoadNetwork& network, std::span<const RouteStep> route, float start_distance, float spacing);
}
