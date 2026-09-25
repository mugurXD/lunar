#include <trok/world/towns.hpp>

#include <optional>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr int32_t TOWN_LINK_REACH = 2;

		const glm::ivec2 LINK_DIRECTIONS[] = { { 1, 0 }, { 0, 1 } };
	}

	Towns::Towns(std::shared_ptr<const RegionPlanner> planner, lunar::World::WorldSettings settings) noexcept
		: planner(std::move(planner)),
		settings(std::move(settings))
	{
	}

	TownSurvey Towns::survey(lunar::World::RegionCoord centre, int32_t radius) const
	{
		const int32_t                         side = radius * 2 + 1;
		std::vector<std::optional<glm::vec2>> towns(static_cast<size_t>(side) * side);
		TownSurvey                            survey;

		const auto town_at = [&](int32_t x, int32_t z) -> std::optional<glm::vec2>& {
			return towns[static_cast<size_t>(z + radius) * side + (x + radius)];
		};

		for (int32_t z = -radius; z <= radius; z++)
		{
			for (int32_t x = -radius; x <= radius; x++)
			{
				const lunar::World::RegionCoord coord = { centre.x + x, centre.z + z };
				const auto                      plan  = std::make_shared<const RegionPlan>(planner->plan(coord, settings));

				survey.regions.push_back({ coord, plan });
				if (!plan->settlements.empty())
					town_at(x, z) = glm::vec2(plan->settlements.front().center);
			}
		}

		for (int32_t z = -radius; z <= radius; z++)
		{
			for (int32_t x = -radius; x <= radius; x++)
			{
				if (!town_at(x, z).has_value())
					continue;

				for (const glm::ivec2& direction : LINK_DIRECTIONS)
				{
					for (int32_t reach = 1; reach <= TOWN_LINK_REACH; reach++)
					{
						const int32_t other_x = x + direction.x * reach;
						const int32_t other_z = z + direction.y * reach;
						if (other_x > radius || other_z > radius)
							break;

						if (!town_at(other_x, other_z).has_value())
							continue;

						survey.links.push_back({ *town_at(x, z), *town_at(other_x, other_z) });
						break;
					}
				}
			}
		}

		return survey;
	}
}
