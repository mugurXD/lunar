#pragma once
#include <trok/world/road.hpp>
#include <trok/world/road_planner.hpp>
#include <trok/world/terrain_generator.hpp>
#include <trok/world/towns.hpp>
#include <lunar/core/jobs.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace trok
{
	constexpr size_t DEFAULT_LINKS_IN_FLIGHT = 1;

	class RoadService
	{
	public:
		RoadService(lunar::JobSystem&                 jobs,
		            std::shared_ptr<TerrainGenerator> generator,
		            RoadClass                         road_class,
		            RoadPlannerSettings               settings,
		            size_t                            max_links_in_flight = DEFAULT_LINKS_IN_FLIGHT) noexcept;
		~RoadService() noexcept;

		RoadService(const RoadService&)            = delete;
		RoadService& operator=(const RoadService&) = delete;

		void                                plan(std::span<const RoadLink> links, const RegionContext& context);
		void                                request(std::span<const TownLink> links, const RegionContext& context);
		std::vector<std::vector<glm::vec3>> update(const glm::vec2& focus);
		std::shared_ptr<const RoadNetwork>  getNetwork() const;

	private:
		struct LinkProgress
		{
			std::vector<std::vector<glm::vec3>> pieces    = {};
			size_t                              remaining = 0;
			bool                                failed    = false;
		};

		struct QueuedLink
		{
			RoadLink      road    = {};
			RegionContext context;
		};

		struct JoinPoint
		{
			glm::vec3               position   = {};
			std::optional<uint32_t> splitRoad  = std::nullopt;
			uint32_t                splitPoint = 0;
		};

		struct PlannedLink
		{
			RoadLink                   road    = {};
			std::optional<RoadArrival> arrival = std::nullopt;
			std::optional<JoinPoint>   join    = std::nullopt;
		};

		bool                       isKnown(const TownLink& link) const;
		std::optional<PlannedLink> routeOf(const RoadLink& link) const;
		void                       startNearest(const glm::vec2& focus);
		void                       planLink(const PlannedLink& link, const HeightSampler& sampler);
		void                       planSegments(std::vector<RoadSegment> segments, const HeightSampler& sampler, std::optional<JoinPoint> join);
		void finishLink();

		lunar::JobSystem&                       jobs;
		std::shared_ptr<const TerrainGenerator> generator;
		RoadClass                               roadClass;
		RoadPlannerSettings                     settings;
		std::shared_ptr<RoadLayer>              layer;
		std::vector<std::vector<glm::vec3>>     roads;
		std::vector<RoadId>                     roadIds;
		std::vector<lunar::JobHandle>           handles;
		std::vector<QueuedLink>                 queue;
		std::vector<TownLink>                   known;
		size_t                                  maxLinksInFlight = 0;
		size_t                                  pendingLinks     = 0;
		size_t                                  publishedRoads   = 0;
		RoadId                                  nextRoadId       = 0;
		bool                                    changed          = false;
	};
}
