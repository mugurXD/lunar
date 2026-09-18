#pragma once
#include <lunar/api.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/world_settings.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace lunar::World
{
	template<typename Plan>
	class RegionPlanner
	{
	public:
		RegionPlanner()          noexcept = default;
		virtual ~RegionPlanner() noexcept = default;

		RegionPlanner(const RegionPlanner&)            = delete;
		RegionPlanner& operator=(const RegionPlanner&) = delete;

		virtual Plan plan(RegionCoord coord, const WorldSettings& settings) const = 0;

		virtual Plan restore(Plan loaded, RegionCoord, const WorldSettings&) const
		{
			return loaded;
		}
	};

	template<typename Plan>
	class RegionContext
	{
	public:
		struct Region
		{
			RegionCoord                 coord = {};
			std::shared_ptr<const Plan> plan  = nullptr;
		};

		RegionContext(const WorldSettings& settings, std::vector<Region> regions) noexcept
			: settings(settings),
			regions(std::move(regions))
		{
		}

		const Plan* findRegion(RegionCoord coord) const
		{
			const auto found = std::ranges::find(regions, coord, &Region::coord);
			return found != regions.end() ? found->plan.get() : nullptr;
		}

		const WorldSettings& getSettings() const
		{
			return settings;
		}

	private:
		WorldSettings       settings;
		std::vector<Region> regions;
	};
}
