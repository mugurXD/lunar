#pragma once
#include <trok/world/biome.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>

#include <lunar/world/grid.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <unordered_map>

struct ImDrawList;

namespace trok
{
	class WorldMapWindow
	{
	public:
		WorldMapWindow(std::shared_ptr<const BiomeLibrary>  biomes,
		               std::shared_ptr<const RegionPlanner> planner,
		               lunar::World::WorldSettings          settings) noexcept;

		void draw(const glm::vec3& viewer, const RoadNetwork* roads);

	private:
		struct View
		{
			glm::vec2  origin = {};
			glm::vec2  size   = {};
			glm::dvec2 center = {};
			float      scale  = 1.f;
		};

		std::shared_ptr<const RegionPlan> findPlan(lunar::World::RegionCoord coord);
		std::vector<RegionContext::Region> gatherVisible(const View& view);

		void drawBiomes(ImDrawList& drawing, const View& view, const RegionContext& context);
		void drawSettlements(ImDrawList& drawing, const View& view, const std::vector<RegionContext::Region>& visible);
		void drawRoads(ImDrawList& drawing, const View& view, const RoadNetwork& roads);

		std::shared_ptr<const BiomeLibrary>  biomes;
		std::shared_ptr<const RegionPlanner> planner;
		lunar::World::WorldSettings          settings;

		std::unordered_map<lunar::World::RegionCoord, std::shared_ptr<const RegionPlan>, lunar::World::GridCoordHash> plans;

		glm::dvec2 center      = {};
		float      span        = 8000.f;
		bool       following   = true;
		uint32_t   plannedHere = 0;
	};
}
