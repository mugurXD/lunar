#pragma once
#include <trok/world/region_plan.hpp>
#include <lunar/world/terrain_generator.hpp>

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
}
