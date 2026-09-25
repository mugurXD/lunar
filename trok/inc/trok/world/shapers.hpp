#pragma once
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <memory>
#include <vector>

namespace trok
{
	using TerrainShaper = lunar::World::TerrainShaper<RegionPlan>;

	class RiverShaper final : public TerrainShaper
	{
	public:
		void declare(const RegionContext&                         context,
		             const glm::dvec2&                            minimum,
		             const glm::dvec2&                            maximum,
		             std::vector<lunar::World::ShapeDeclaration>& output) const override;
	};

	class RoadShaper final : public TerrainShaper
	{
	public:
		explicit RoadShaper(std::shared_ptr<const RoadLayer> roads) noexcept;

		void declare(const RegionContext&                         context,
		             const glm::dvec2&                            minimum,
		             const glm::dvec2&                            maximum,
		             std::vector<lunar::World::ShapeDeclaration>& output) const override;

	private:
		std::shared_ptr<const RoadLayer> roads;
	};
}
