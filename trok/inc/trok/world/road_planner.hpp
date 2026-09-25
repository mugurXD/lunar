#pragma once
#include <trok/world/road.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace trok
{
	using HeightSampler = lunar::World::HeightSampler;

	struct RoadPlannerSettings
	{
		int    headings         = 16;
		float  heightStep       = 1.f;
		float  heuristicWeight  = 1.5f;
		float  pointSpacing     = 6.f;
		float  seaLevel         = -std::numeric_limits<float>::infinity();
		float  searchMargin     = 600.f;
		float  segmentThreshold = 2000.f;
		float  segmentLength    = 1500.f;
		float  coarseScale      = 8.f;
		size_t maxNodes         = 1500000;
		size_t coarseMaxNodes   = 1500000;
	};

	struct RoadWaypoint
	{
		glm::vec2          position = {};
		float              height   = 0.f;
		std::optional<int> heading  = std::nullopt;
	};

	struct RoadSegment
	{
		RoadWaypoint           from     = {};
		RoadWaypoint           to       = {};
		std::vector<glm::vec3> fallback = {};
	};

	std::optional<std::vector<RoadSegment>> SplitRoad(const glm::vec2&           start,
	                                                  const glm::vec2&           end,
	                                                  const RoadClass&           road_class,
	                                                  const HeightSampler&       sample_height,
	                                                  const RoadPlannerSettings& settings = {});

	std::optional<std::vector<glm::vec3>> PlanSegment(const RoadSegment&         segment,
	                                                  const RoadClass&           road_class,
	                                                  const HeightSampler&       sample_height,
	                                                  const RoadPlannerSettings& settings = {});

	std::vector<glm::vec3> JoinSegments(std::span<const std::vector<glm::vec3>> pieces, const RoadPlannerSettings& settings = {});

	std::optional<std::vector<glm::vec3>> PlanRoad(const glm::vec2&           start,
	                                               const glm::vec2&           end,
	                                               const RoadClass&           road_class,
	                                               const HeightSampler&       sample_height,
	                                               const RoadPlannerSettings& settings = {});
}
