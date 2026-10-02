#pragma once
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace trok
{
	struct TownLink
	{
		lunar::World::RegionCoord from = {};
		lunar::World::RegionCoord to   = {};
		RoadLink                  road = {};
	};

	struct TownSurvey
	{
		std::vector<Settlement>            towns   = {};
		std::vector<TownLink>              links   = {};
		std::vector<RegionContext::Region> regions = {};
	};

	class Towns
	{
	public:
		Towns(std::shared_ptr<const RegionPlanner> planner, lunar::World::WorldSettings settings) noexcept;

		std::optional<Settlement> townIn(lunar::World::RegionCoord coord)       const;
		std::vector<TownLink>     linksOwnedBy(lunar::World::RegionCoord coord) const;
		TownSurvey                survey(lunar::World::RegionCoord centre, int32_t radius) const;

	private:
		std::shared_ptr<const RegionPlanner> planner;
		lunar::World::WorldSettings          settings;
	};
}
