#pragma once
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace trok
{
	struct TownSurvey
	{
		std::vector<Settlement>            towns   = {};
		std::vector<RoadLink>              links   = {};
		std::vector<RegionContext::Region> regions = {};
	};

	class Towns
	{
	public:
		Towns(std::shared_ptr<const RegionPlanner> planner, lunar::World::WorldSettings settings) noexcept;

		TownSurvey survey(lunar::World::RegionCoord centre, int32_t radius) const;

	private:
		std::shared_ptr<const RegionPlanner> planner;
		lunar::World::WorldSettings          settings;
	};
}
