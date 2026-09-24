#pragma once
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace trok
{
	constexpr size_t QUAD_CORNERS = 4;

	using TerrainDresser = lunar::World::TerrainDresser<RegionPlan>;

	class SeaDresser final : public TerrainDresser
	{
	public:
		explicit SeaDresser(float sea_level) noexcept;

		void dress(const RegionContext&                    context,
		           lunar::World::ChunkCoord                coord,
		           const lunar::World::WorldSettings&      settings,
		           const lunar::World::HeightSampler&      ground,
		           std::vector<lunar::World::DressedMesh>& output) const override;

	private:
		float seaLevel = 0.f;
	};

	class RiverWaterDresser final : public TerrainDresser
	{
	public:
		void dress(const RegionContext&                    context,
		           lunar::World::ChunkCoord                coord,
		           const lunar::World::WorldSettings&      settings,
		           const lunar::World::HeightSampler&      ground,
		           std::vector<lunar::World::DressedMesh>& output) const override;
	};

	class RoadDresser final : public TerrainDresser
	{
	public:
		explicit RoadDresser(std::shared_ptr<const RoadLayer> roads) noexcept;

		void dress(const RegionContext&                    context,
		           lunar::World::ChunkCoord                coord,
		           const lunar::World::WorldSettings&      settings,
		           const lunar::World::HeightSampler&      ground,
		           std::vector<lunar::World::DressedMesh>& output) const override;

	private:
		std::shared_ptr<const RoadLayer> roads;
	};
}
