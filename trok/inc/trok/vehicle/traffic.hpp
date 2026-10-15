#pragma once
#include <trok/vehicle/vehicle.hpp>
#include <trok/world/drive_planner.hpp>
#include <trok/world/region_plan.hpp>
#include <trok/world/road.hpp>
#include <trok/world/road_routes.hpp>
#include <trok/world/road_service.hpp>

#include <lunar/core/engine.hpp>
#include <lunar/world/terrain_colliders.hpp>
#include <lunar/world/terrain_world.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace trok
{
	struct TrafficFocus
	{
	};

	struct TrafficCar
	{
		std::unique_ptr<Vehicle> vehicle = nullptr;
	};

	struct Trip
	{
		std::vector<glm::vec3> checkpoints = {};
		size_t                 next        = 0;
	};

	struct DrivePath
	{
		std::vector<glm::vec3> points    = {};
		std::vector<float>     distances = {};
		std::vector<float>     speeds    = {};
		size_t                 segment   = 0;
		float                  progress  = 0.f;
		float                  replanIn  = 0.f;
		float                  lostFor   = 0.f;
	};

	struct Leader
	{
		float gap   = std::numeric_limits<float>::infinity();
		float speed = 0.f;
	};

	struct DriverSettings
	{
		float minimumGap          = 2.f;
		float timeHeadway         = 1.5f;
		float acceleration        = 1.5f;
		float comfortableBraking  = 2.f;
		float lateralAcceleration = 2.f;
		float curvatureSpan       = 20.f;
		float offRoadSpeed        = 8.f;
		float manoeuvreSpeed      = 4.f;
		float pedalAcceleration   = 6.f;
		float pedalBraking        = 8.f;
		float lookaheadTime       = 0.8f;
		float minimumLookahead    = 6.f;
		float scanDistance        = 120.f;
	};

	struct TrafficSettings
	{
		size_t               vehicleCount           = 200;
		float                spawnRadius            = 50.f;
		float                despawnRadius          = 200.f;
		float                spawnSpacing           = 10.f;
		float                roadSearch             = 40.f;
		float                destinationRadius      = 20000.f;
		float                minimumTrip            = 300.f;
		float                townChance             = 0.8f;
		float                checkpointSpacing      = 25.f;
		float                checkpointReach        = 8.f;
		size_t               anticipatedCheckpoints = 6;
		float                planReach              = 80.f;
		float                replanInterval         = 1.f;
		size_t               plansPerFrame          = 4;
		float                lostTimeout            = 10.f;
		float                lostDespawnRadius      = 150.f;
		DriverSettings       driver                 = {};
		DrivePlannerSettings planner                = {};
	};

	class Traffic
	{
	public:
		Traffic(lunar::Engine&                        engine,
		        const RoadService&                    roads,
		        const lunar::World::TerrainWorld&     terrain,
		        const lunar::World::TerrainColliders& colliders,
		        const lunar::World::WorldSettings&    world,
		        VehicleDefinition                     definition,
		        uint64_t                              seed,
		        TrafficSettings                       settings = {}) noexcept;

		Traffic(const Traffic&)            = delete;
		Traffic& operator=(const Traffic&) = delete;

		void setTowns(std::vector<Settlement> known_towns);

	private:
		void                  fixedUpdate(lunar::Scene& scene, const lunar::FrameTime& frame_time);
		void                  update(lunar::Scene& scene, const lunar::FrameTime& frame_time);
		void                  perceive(lunar::Scene& scene, const RoadNetwork& network)                         const;
		void                  drive(lunar::Scene& scene, float delta_time)                                      const;
		void                  populate(lunar::Scene& scene, const RoadNetwork& network);
		void                  spawn(lunar::Scene& scene, const RoadNetwork& network, const glm::vec3& focus);
		void                  navigate(lunar::Scene& scene, const RoadNetwork& network);
		void                  replan(lunar::Scene& scene, const RoadNetwork& network, float delta_time);
		void                  present(lunar::Scene& scene, float alpha);
		std::optional<Trip>   continueTrip(const RoadNetwork& network, const lunar::Transform& transform);
		std::optional<Trip>   planTrip(const RoadNetwork& network, const RouteStep& from, float start_distance);
		std::optional<size_t> pickDestination(const RoadNetwork& network, const glm::vec3& origin);
		bool                  isCrowded(lunar::Scene& scene, const glm::vec3& position)                         const;

		lunar::Engine&                        engine;
		const RoadService&                    roads;
		const lunar::World::TerrainColliders& colliders;
		VehicleDefinition                     definition;
		TrafficSettings                       settings;
		DrivePlanner                          planner;
		std::mt19937_64                       random;
		std::vector<Settlement>               towns = {};
	};
}
