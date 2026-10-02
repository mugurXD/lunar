#include <trok/world/towns.hpp>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr int32_t TOWN_LINK_REACH = 2;

		const glm::ivec2 LINK_DIRECTIONS[] = { { 1, 0 }, { 0, 1 } };

		std::optional<Settlement> FirstTown(const RegionPlan& plan)
		{
			return plan.settlements.empty() ? std::nullopt : std::optional<Settlement>(plan.settlements.front());
		}

		template<typename TownAt>
		std::vector<TownLink> LinksFrom(lunar::World::RegionCoord coord, TownAt&& town_at)
		{
			const std::optional<Settlement> town = town_at(coord);
			if (!town.has_value())
				return {};

			std::vector<TownLink> links;
			for (const glm::ivec2& direction : LINK_DIRECTIONS)
			{
				for (int32_t reach = 1; reach <= TOWN_LINK_REACH; reach++)
				{
					const lunar::World::RegionCoord other_coord = { coord.x + direction.x * reach, coord.z + direction.y * reach };
					const std::optional<Settlement> other       = town_at(other_coord);
					if (!other.has_value())
						continue;

					links.push_back({ .from = coord, .to = other_coord, .road = { glm::vec2(town->center), glm::vec2(other->center) } });
					break;
				}
			}

			return links;
		}
	}

	Towns::Towns(std::shared_ptr<const RegionPlanner> planner, lunar::World::WorldSettings settings) noexcept
		: planner(std::move(planner)),
		settings(std::move(settings))
	{
	}

	std::optional<Settlement> Towns::townIn(lunar::World::RegionCoord coord) const
	{
		return FirstTown(planner->plan(coord, settings));
	}

	std::vector<TownLink> Towns::linksOwnedBy(lunar::World::RegionCoord coord) const
	{
		return LinksFrom(coord, [this](lunar::World::RegionCoord other) { return townIn(other); });
	}

	TownSurvey Towns::survey(lunar::World::RegionCoord centre, int32_t radius) const
	{
		std::unordered_map<lunar::World::RegionCoord, std::shared_ptr<const RegionPlan>, lunar::World::GridCoordHash> plans;

		const auto plan_of = [&](lunar::World::RegionCoord coord) {
			auto found = plans.find(coord);
			if (found == plans.end())
				found = plans.emplace(coord, std::make_shared<const RegionPlan>(planner->plan(coord, settings))).first;

			return found->second;
		};

		const auto town_at = [&](lunar::World::RegionCoord coord) { return FirstTown(*plan_of(coord)); };

		TownSurvey survey;
		const auto add_town = [&](lunar::World::RegionCoord coord) {
			const std::optional<Settlement> town = town_at(coord);
			if (town.has_value() && std::ranges::find(survey.towns, *town) == survey.towns.end())
				survey.towns.push_back(*town);
		};

		for (int32_t z = -radius; z <= radius; z++)
		{
			for (int32_t x = -radius; x <= radius; x++)
			{
				const lunar::World::RegionCoord coord = { centre.x + x, centre.z + z };

				survey.regions.push_back({ coord, plan_of(coord) });
				add_town(coord);

				for (TownLink& link : LinksFrom(coord, town_at))
				{
					add_town(link.to);
					survey.links.push_back(std::move(link));
				}
			}
		}

		return survey;
	}
}
