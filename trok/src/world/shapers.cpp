#include <trok/world/shapers.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace trok
{
	namespace
	{
		bool Reaches(const River& river, const glm::dvec2& minimum, const glm::dvec2& maximum)
		{
			const double reach = RiverReach();

			return river.maximum.x + reach >= minimum.x && river.minimum.x - reach <= maximum.x &&
			       river.maximum.y + reach >= minimum.y && river.minimum.y - reach <= maximum.y;
		}
	}

	void RiverShaper::declare(const RegionContext&                         context,
	                          const glm::dvec2&                            minimum,
	                          const glm::dvec2&                            maximum,
	                          std::vector<lunar::World::ShapeDeclaration>& output) const
	{
		const lunar::World::WorldSettings& settings = context.getSettings();
		const lunar::World::RegionCoord    lowest   = lunar::World::RegionAt(lunar::World::ChunkAt(minimum.x, minimum.y, settings), settings);
		const lunar::World::RegionCoord    highest  = lunar::World::RegionAt(lunar::World::ChunkAt(maximum.x, maximum.y, settings), settings);

		for (int32_t z = lowest.z; z <= highest.z; z++)
		{
			for (int32_t x = lowest.x; x <= highest.x; x++)
			{
				const RegionPlan* plan = context.findRegion({ x, z });
				if (plan == nullptr)
					continue;

				for (const River& river : plan->rivers)
					if (Reaches(river, minimum, maximum))
						std::ranges::move(RiverShapes(river), std::back_inserter(output));
			}
		}
	}

	RoadShaper::RoadShaper(std::shared_ptr<const RoadLayer> roads) noexcept
		: roads(std::move(roads))
	{
	}

	void RoadShaper::declare(const RegionContext&,
	                         const glm::dvec2&                            minimum,
	                         const glm::dvec2&                            maximum,
	                         std::vector<lunar::World::ShapeDeclaration>& output) const
	{
		const std::shared_ptr<const RoadNetwork> network = roads->get();
		if (network != nullptr)
			std::ranges::move(network->shapesReaching(glm::vec2(minimum), glm::vec2(maximum)), std::back_inserter(output));
	}
}
