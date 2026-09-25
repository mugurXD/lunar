#pragma once
#include <trok/world/road.hpp>
#include <trok/world/road_planner.hpp>
#include <trok/world/terrain_generator.hpp>
#include <lunar/core/jobs.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace trok
{
	class RoadService
	{
	public:
		RoadService(lunar::JobSystem&                 jobs,
		            std::shared_ptr<TerrainGenerator> generator,
		            RoadClass                         road_class,
		            RoadPlannerSettings               settings) noexcept;
		~RoadService() noexcept;

		RoadService(const RoadService&)            = delete;
		RoadService& operator=(const RoadService&) = delete;

		void                               plan(std::span<const RoadLink> links, const RegionContext& context);
		bool                               update();
		std::shared_ptr<const RoadNetwork> getNetwork() const;

	private:
		struct LinkProgress
		{
			std::vector<std::vector<glm::vec3>> pieces    = {};
			size_t                              remaining = 0;
			bool                                failed    = false;
		};

		void planLink(const RoadLink& link, const HeightSampler& sampler);
		void planSegments(std::vector<RoadSegment> segments, const HeightSampler& sampler);
		void finishLink();

		lunar::JobSystem&                       jobs;
		std::shared_ptr<const TerrainGenerator> generator;
		RoadClass                               roadClass;
		RoadPlannerSettings                     settings;
		std::shared_ptr<RoadLayer>              layer;
		std::vector<std::vector<glm::vec3>>     roads;
		std::vector<lunar::JobHandle>           handles;
		size_t                                  pendingLinks = 0;
		bool                                    changed      = false;
	};
}
