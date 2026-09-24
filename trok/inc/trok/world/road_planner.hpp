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
		float  step                = 12.f;
		int    headings            = 16;
		float  pointSpacing        = 6.f;
		int    refinementPasses    = 40;
		float  smoothingWeight     = 0.25f;
		float  terrainWeight       = 0.05f;
		float  maxRefinementStep   = 2.f;
		int    profilePasses       = 24;
		float  groundWeight        = 0.35f;
		float  seaLevel            = -std::numeric_limits<float>::infinity();
		float  waterPenalty        = 5.f;
		float  searchMargin        = 600.f;
		size_t maxNodes            = 1500000;
	};

	std::optional<std::vector<glm::vec3>> PlanRoad(const glm::vec2&           start,
	                                               const glm::vec2&           end,
	                                               const RoadClass&           road_class,
	                                               const HeightSampler&       sample_height,
	                                               const RoadPlannerSettings& settings = {});
}
