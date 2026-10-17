#pragma once
#include <trok/vehicle/car_following.hpp>
#include <trok/vehicle/traffic_spawning.hpp>
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace trok
{
	struct TrafficFocus
	{
	};

	struct TrafficCar
	{
		std::unique_ptr<Vehicle> vehicle    = nullptr;
		float                    stalledFor = 0.f;
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

	struct TrafficSettings
	{
		float                defaultDensity         = 100.f;
		float                densityRadius          = 200.f;
		float                surplusFactor          = 1.5f;
		float                cullInterval           = 1.f;
		float                spawnAttemptsPerSecond = 100.f;
		float                nearDistance           = 80.f;
		float                spawnRadius            = 500.f;
		float                despawnRadius          = 600.f;
		float                fogConcealment         = 0.5f;
		float                viewAngle              = 75.f;
		float                junctionClearance      = 30.f;
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
		float                stallSpeed             = 1.f;
		float                stallTimeout           = 30.f;
		DriverSettings       driver                 = {};
		DrivePlannerSettings planner                = {};
	};

	enum class SpawnOutcome : uint8_t
	{
		eSpawned,
		eNoSpot,
		eOutOfRange,
		eInView,
		eUnloadedTerrain,
		eCrowded,
		eNoTrip,
		eNoCourse,
		eNoGap,
		eCount
	};

	enum class DespawnReason : uint8_t
	{
		eLost,
		eStalled,
		eFrozen,
		eFar,
		eSurplus,
		eCount
	};

	struct TrafficCounters
	{
		std::array<uint64_t, std::to_underlying(SpawnOutcome::eCount)>  spawns         = {};
		std::array<uint64_t, std::to_underlying(DespawnReason::eCount)> despawns       = {};
		uint64_t                                                        tripsContinued = 0;
		uint64_t                                                        tripsStranded  = 0;
		uint64_t                                                        plans          = 0;
		uint64_t                                                        failedPlans    = 0;
		float                                                           elapsed        = 0.f;
	};

	struct TrafficSnapshot
	{
		size_t vehicles          = 0;
		size_t frozen            = 0;
		size_t stalled           = 0;
		size_t lost              = 0;
		size_t tailgating        = 0;
		size_t awaitingReplan    = 0;
		size_t awaitingDespawn   = 0;
		size_t nearFoci          = 0;
		float  targetNearFoci    = 0.f;
		float  longestStall      = 0.f;
		float  longestLost       = 0.f;
		float  longestReplanWait = 0.f;
		float  closestGap        = std::numeric_limits<float>::infinity();
		float  meanSpeed         = 0.f;
		float  meanDesiredSpeed  = 0.f;
		float  meanDeviation     = 0.f;
		float  largestDeviation  = 0.f;
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

		void                   setTowns(std::vector<Settlement> known_towns);
		void                   setDensity(TrafficDensity sampler);
		void                   resetCounters();
		const TrafficCounters& getCounters()                const;
		TrafficSnapshot        inspect(lunar::Scene& scene) const;

	private:
		void                         fixedUpdate(lunar::Scene& scene, const lunar::FrameTime& frame_time);
		void                         update(lunar::Scene& scene, const lunar::FrameTime& frame_time);
		void                         perceive(lunar::Scene& scene, const RoadNetwork& network)                                                                         const;
		void                         drive(lunar::Scene& scene, float delta_time)                                                                                      const;
		void                         populate(lunar::Scene& scene, const RoadNetwork& network, float delta_time);
		void                         despawn(lunar::Scene& scene, const TrafficViewer& viewer, std::span<const glm::vec3> foci);
		void                         cullSurplus(lunar::Scene& scene, const RoadNetwork& network, const TrafficViewer& viewer, std::span<const LaneVehicle> vehicles);
		SpawnOutcome                 spawn(lunar::Scene& scene, const RoadNetwork& network, const TrafficViewer& viewer, const glm::vec3& focus, std::vector<LaneVehicle>& vehicles);
		void                         navigate(lunar::Scene& scene, const RoadNetwork& network);
		void                         replan(lunar::Scene& scene, const RoadNetwork& network, float delta_time);
		void                         planCourse(DrivePath& path, const Trip& trip, const glm::vec3& position, const RoadNetwork& network);
		void                         present(lunar::Scene& scene, float alpha);
		std::optional<TrafficViewer> viewerOf(lunar::Scene& scene)                                                                                                     const;
		std::optional<LaneSpot>      pickSpot(const RoadNetwork& network, const glm::vec3& focus);
		std::optional<Trip>          continueTrip(const RoadNetwork& network, const lunar::Transform& transform);
		std::optional<Trip>          planTrip(const RoadNetwork& network, const RouteStep& from, float start_distance);
		std::optional<size_t>        pickDestination(const RoadNetwork& network, const glm::vec3& origin);
		std::optional<DespawnReason> despawnReason(const TrafficCar& car, const DrivePath& path, std::span<const glm::vec3> foci)                                      const;
		float                        targetNear(const RoadNetwork& network, const glm::vec3& position)                                                                 const;
		bool                         isCrowded(const RoadNetwork& network, std::span<const LaneVehicle> vehicles, const glm::vec3& position, float allowance)        const;

		lunar::Engine&                        engine;
		const RoadService&                    roads;
		const lunar::World::TerrainColliders& colliders;
		VehicleDefinition                     definition;
		TrafficSettings                       settings;
		DrivePlanner                          planner;
		std::mt19937_64                       random;
		TrafficDensity                        density;
		std::vector<Settlement>               towns         = {};
		TrafficCounters                       counters      = {};
		float                                 spawnAttempts = 0.f;
		float                                 cullIn        = 0.f;
	};
}
