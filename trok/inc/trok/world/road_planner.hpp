#pragma once
#include <trok/world/road.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace trok
{
	using HeightSampler = lunar::World::HeightSampler;

	struct RoadPlannerSettings
	{
		int    headings        = 16;
		float  heightStep      = 1.f;
		float  heuristicWeight = 1.5f;
		float  pointSpacing    = 6.f;
		float  seaLevel        = -std::numeric_limits<float>::infinity();
		float  searchMargin    = 600.f;
		size_t maxNodes        = 1500000;
	};

	std::optional<std::vector<glm::vec3>> PlanRoad(const glm::vec2&           start,
	                                               const glm::vec2&           end,
	                                               const RoadClass&           road_class,
	                                               const HeightSampler&       sample_height,
	                                               const RoadPlannerSettings& settings = {});
}
