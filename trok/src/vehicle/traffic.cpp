#include <trok/vehicle/traffic.hpp>

#include <lunar/physics/conversions.hpp>
#include <lunar/utils/geometry.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr float HALF               = 0.5f;
		constexpr float SPAWN_HEIGHT       = 1.5f;
		constexpr float IDM_EXPONENT       = 4.f;
		constexpr float MIN_GAP            = 0.1f;
		constexpr float MIN_REACH          = 1.f;
		constexpr float MIN_SPEED          = 0.1f;
		constexpr float MIN_SEGMENT_LENGTH = 1e-4f;
		constexpr float MIN_TURN_AREA      = 1e-4f;
		constexpr float HOLD_SPEED         = 0.5f;
		constexpr float FULL_LOCK          = 1.f;
		constexpr float BODY_CELL          = 16.f;
		constexpr float DEBUG_LIFT         = 0.3f;

		const glm::vec3                    UP              = { 0.f, 1.f, 0.f };
		const glm::vec3                    CHASSIS_FORWARD = { 0.f, 0.f, -1.f };
		const glm::quat                    IDENTITY        = glm::quat(1.f, 0.f, 0.f, 0.f);
		const glm::vec4                    PATH_COLOR      = { 0.f, 0.f, 1.f, 1.f };
		const glm::vec4                    ROUTE_COLOR     = { 1.f, 0.8f, 0.f, 1.f };
		const lunar::Physics::VehicleInput BRAKING         = { .brake = 1.f };

		using BodyCell = lunar::World::GridCoord<struct TrafficBodyTag>;
		using BodyGrid = std::unordered_map<BodyCell, std::vector<size_t>, lunar::World::GridCoordHash>;

		struct Body
		{
			lunar::Entity entity     = nullptr;
			glm::vec3     position   = {};
			glm::vec3     velocity   = {};
			float         halfLength = 0.f;
		};

		glm::vec2 Flat(const glm::vec3& point)
		{
			return { point.x, point.z };
		}

		BodyCell BodyCellAt(const glm::vec2& position)
		{
			return { static_cast<int32_t>(std::floor(position.x / BODY_CELL)), static_cast<int32_t>(std::floor(position.y / BODY_CELL)) };
		}

		std::optional<float> GroundAt(const lunar::World::TerrainWorld& terrain, const lunar::World::WorldSettings& world, const glm::vec2& position)
		{
			const lunar::World::ChunkCoord coord     = lunar::World::ChunkAt(position.x, position.y, world);
			const lunar::World::Heightmap* heightmap = terrain.findHeightmap(coord);
			if (heightmap == nullptr)
				return std::nullopt;

			const glm::vec2  sample    = (position - Flat(lunar::World::ChunkOrigin(coord, world))) / world.vertexSpacing;
			const glm::ivec2 base      = glm::ivec2(glm::floor(sample));
			const glm::vec2  amount    = sample - glm::vec2(base);
			const float      lower_row = glm::mix(heightmap->sample(base.x, base.y), heightmap->sample(base.x + 1, base.y), amount.x);
			const float      upper_row = glm::mix(heightmap->sample(base.x, base.y + 1), heightmap->sample(base.x + 1, base.y + 1), amount.x);
			return glm::mix(lower_row, upper_row, amount.y);
		}

		float DistanceToNearest(std::span<const glm::vec3> foci, const glm::vec3& position)
		{
			return std::ranges::min(foci | std::views::transform([&](const glm::vec3& focus) { return glm::distance(Flat(focus), Flat(position)); }));
		}

		std::vector<float> DistancesAlong(std::span<const glm::vec3> course)
		{
			std::vector<float> distances(course.size(), 0.f);
			for (size_t point = 1; point < course.size(); point++)
				distances[point] = distances[point - 1] + glm::distance(course[point - 1], course[point]);

			return distances;
		}

		float TurnRadius(const glm::vec3& before, const glm::vec3& at, const glm::vec3& after)
		{
			const glm::vec2 first  = Flat(at) - Flat(before);
			const glm::vec2 second = Flat(after) - Flat(before);
			const float     cross  = std::abs(first.x * second.y - first.y * second.x);
			if (cross <= MIN_TURN_AREA)
				return std::numeric_limits<float>::infinity();

			return glm::length(first) * glm::length(second) * glm::distance(Flat(at), Flat(after)) / (2.f * cross);
		}

		std::vector<float> SpeedProfile(std::span<const glm::vec3> course, std::span<const float> distances, std::vector<float> speeds, const DriverSettings& settings)
		{
			const float reach  = settings.curvatureSpan * HALF;
			size_t      before = 0;
			size_t      after  = 0;
			for (size_t point = 1; point + 1 < course.size(); point++)
			{
				while (before + 1 < point && distances[point] - distances[before + 1] >= reach)
					before++;

				after = std::max(after, point + 1);
				while (after + 1 < course.size() && distances[after] - distances[point] < reach)
					after++;

				speeds[point] = std::min(speeds[point], std::sqrt(settings.lateralAcceleration * TurnRadius(course[before], course[point], course[after])));
			}

			for (size_t point = course.size() - 1; point-- > 0;)
			{
				const float braking = 2.f * settings.comfortableBraking * (distances[point + 1] - distances[point]);
				speeds[point]       = std::min(speeds[point], std::sqrt(speeds[point + 1] * speeds[point + 1] + braking));
			}

			return speeds;
		}

		void SetCourse(DrivePath& path, std::span<const DrivePoint> route, std::span<const glm::vec3> ahead, float speed_limit, const TrafficSettings& settings)
		{
			path = DrivePath { .replanIn = settings.replanInterval, .lostFor = path.lostFor };
			if (route.size() < 2)
				return;

			std::vector<glm::vec3> course;
			std::vector<float>     caps;
			for (const DrivePoint& point : route)
			{
				course.push_back(point.position);
				caps.push_back(point.onRoad ? speed_limit : settings.driver.offRoadSpeed);
			}

			for (const glm::vec3& checkpoint : ahead.first(std::min(ahead.size(), settings.anticipatedCheckpoints)))
			{
				course.push_back(checkpoint);
				caps.push_back(speed_limit);
			}

			const std::vector<float> distances = DistancesAlong(course);
			const std::vector<float> speeds    = SpeedProfile(course, distances, std::move(caps), settings.driver);
			const auto               kept      = static_cast<std::ptrdiff_t>(route.size());

			path.points.assign(course.begin(), course.begin() + kept);
			path.distances.assign(distances.begin(), distances.begin() + kept);
			path.speeds.assign(speeds.begin(), speeds.begin() + kept);
		}

		glm::vec3 PointAt(const DrivePath& path, float distance)
		{
			if (distance >= path.distances.back())
				return path.points.back();

			const size_t next = static_cast<size_t>(std::ranges::upper_bound(path.distances, distance) - path.distances.begin());
			if (next == 0)
				return path.points.front();

			const size_t previous = next - 1;
			return glm::mix(path.points[previous], path.points[next], (distance - path.distances[previous]) / (path.distances[next] - path.distances[previous]));
		}

		void Advance(DrivePath& path, const glm::vec3& position)
		{
			const std::vector<glm::vec3>& points = path.points;
			while (path.segment + 2 < points.size() && glm::dot(position - points[path.segment + 1], points[path.segment + 2] - points[path.segment + 1]) > 0.f)
				path.segment++;

			const glm::vec3 course = points[path.segment + 1] - points[path.segment];
			const float     amount = std::clamp(glm::dot(position - points[path.segment], course) / std::max(glm::dot(course, course), MIN_SEGMENT_LENGTH), 0.f, 1.f);
			path.progress = glm::mix(path.distances[path.segment], path.distances[path.segment + 1], amount);
		}

		float DesiredSpeed(const DrivePath& path)
		{
			const size_t segment = path.segment;
			const float  amount  = (path.progress - path.distances[segment]) / (path.distances[segment + 1] - path.distances[segment]);
			return std::max(glm::mix(path.speeds[segment], path.speeds[segment + 1], amount), MIN_SPEED);
		}

		float Wheelbase(const lunar::Physics::VehicleSettings& settings)
		{
			const auto [rear, front] = std::ranges::minmax(settings.wheels, {}, [](const lunar::Physics::WheelSettings& wheel) { return wheel.mountPoint.z; });
			return front.mountPoint.z - rear.mountPoint.z;
		}

		float Steering(const Vehicle& vehicle, const DrivePath& path, float speed, const DriverSettings& settings)
		{
			const lunar::Transform&                transform        = vehicle.getTransform();
			const lunar::Physics::VehicleSettings& vehicle_settings = vehicle.getVehicle().getSettings();
			const float                            lookahead        = std::max(settings.minimumLookahead, std::abs(speed) * settings.lookaheadTime);
			const glm::vec3                        local            = glm::inverse(transform.rotation) * (PointAt(path, path.progress + lookahead) - transform.position);
			if (local.z > 0.f)
				return local.x >= 0.f ? FULL_LOCK : -FULL_LOCK;

			const float reach = std::max(local.x * local.x + local.z * local.z, MIN_REACH);
			const float angle = std::atan(Wheelbase(vehicle_settings) * 2.f * local.x / reach);
			return std::clamp(angle / glm::radians(vehicle_settings.maxSteerAngle), -FULL_LOCK, FULL_LOCK);
		}

		float Acceleration(const Leader& leader, float speed, float desired_speed, const DriverSettings& settings)
		{
			const float closing     = speed - leader.speed;
			const float braking_gap = settings.minimumGap + std::max(0.f, speed * settings.timeHeadway + speed * closing / (2.f * std::sqrt(settings.acceleration * settings.comfortableBraking)));
			const float interaction = braking_gap / std::max(leader.gap, MIN_GAP);

			return settings.acceleration * (1.f - std::pow(speed / desired_speed, IDM_EXPONENT) - interaction * interaction);
		}

		lunar::Physics::VehicleInput Decide(const Vehicle& vehicle, DrivePath& path, const Leader& leader, const DriverSettings& settings)
		{
			if (path.points.size() < 2)
				return BRAKING;

			Advance(path, vehicle.getTransform().position);

			const float speed        = vehicle.getVehicle().getForwardSpeed();
			const float steering     = Steering(vehicle, path, speed, settings);
			const float desired      = std::abs(steering) >= FULL_LOCK ? std::min(DesiredSpeed(path), settings.manoeuvreSpeed) : DesiredSpeed(path);
			const float acceleration = Acceleration(leader, std::max(speed, 0.f), desired, settings);
			if (acceleration <= 0.f && speed < HOLD_SPEED)
				return { .brake = 1.f, .steering = steering };

			return lunar::Physics::VehicleInput
			{
				.throttle = std::clamp(acceleration / settings.pedalAcceleration, 0.f, 1.f),
				.brake    = std::clamp(-acceleration / settings.pedalBraking, 0.f, 1.f),
				.steering = steering
			};
		}

		template<typename Visitor>
		void VisitBodies(const BodyGrid& grid, const glm::vec2& minimum, const glm::vec2& maximum, Visitor&& visit)
		{
			const BodyCell lowest  = BodyCellAt(minimum);
			const BodyCell highest = BodyCellAt(maximum);
			for (int32_t z = lowest.z; z <= highest.z; z++)
				for (int32_t x = lowest.x; x <= highest.x; x++)
					if (const auto cell = grid.find({ x, z }); cell != grid.end())
						std::ranges::for_each(cell->second, visit);
		}

		Leader FindLeader(const DrivePath& path, lunar::Entity self, float half_length, std::span<const Body> bodies, const BodyGrid& grid, float tolerance, float scan)
		{
			for (size_t index = path.segment; index + 1 < path.points.size() && path.distances[index] < path.progress + scan; index++)
			{
				const glm::vec2 from    = Flat(path.points[index]);
				const glm::vec2 to      = Flat(path.points[index + 1]);
				const glm::vec3 heading = glm::normalize(path.points[index + 1] - path.points[index]);

				std::optional<Leader> closest;
				VisitBodies(grid, glm::min(from, to) - tolerance, glm::max(from, to) + tolerance, [&](size_t candidate) {
					const Body& body    = bodies[candidate];
					float       amount  = 0.f;
					const float lateral = lunar::DistanceToSegment(Flat(body.position), from, to, amount);
					const float along   = glm::mix(path.distances[index], path.distances[index + 1], amount);
					const float gap     = along - path.progress - half_length - body.halfLength;
					if (body.entity != self && lateral <= tolerance && amount > 0.f && along > path.progress && (!closest.has_value() || gap < closest->gap))
						closest = Leader { .gap = gap, .speed = glm::dot(body.velocity, heading) };
				});

				if (closest.has_value())
					return *closest;
			}

			return {};
		}

		bool HasPassed(const Trip& trip, const glm::vec3& position, float reach)
		{
			const glm::vec2 checkpoint = Flat(trip.checkpoints[trip.next]);
			if (glm::distance(Flat(position), checkpoint) <= reach)
				return true;

			if (trip.next + 1 >= trip.checkpoints.size())
				return false;

			const glm::vec2 following = Flat(trip.checkpoints[trip.next + 1]);
			return glm::distance(Flat(position), following) < glm::distance(checkpoint, following);
		}

		size_t PlanTarget(const Trip& trip, const glm::vec3& position, float reach)
		{
			size_t target    = trip.next;
			float  travelled = glm::distance(Flat(position), Flat(trip.checkpoints[target]));
			while (target + 1 < trip.checkpoints.size())
			{
				travelled += glm::distance(trip.checkpoints[target], trip.checkpoints[target + 1]);
				if (travelled > reach)
					break;

				target++;
			}

			return target;
		}

		void DrawLifted(lunar::Render::Renderer& renderer, std::span<const glm::vec3> points, const glm::vec4& color)
		{
			if (points.size() < 2)
				return;

			std::vector<glm::vec3> lifted;
			std::ranges::transform(points, std::back_inserter(lifted), [](const glm::vec3& point) { return point + UP * DEBUG_LIFT; });
			renderer.drawLine(lifted, color);
		}
	}

	Traffic::Traffic(lunar::Engine&                        engine,
	                 const RoadService&                    roads,
	                 const lunar::World::TerrainWorld&     terrain,
	                 const lunar::World::TerrainColliders& colliders,
	                 const lunar::World::WorldSettings&    world,
	                 VehicleDefinition                     definition,
	                 uint64_t                              seed,
	                 TrafficSettings                       settings) noexcept
		: engine(engine),
		roads(roads),
		colliders(colliders),
		definition(std::move(definition)),
		settings(settings),
		planner([&terrain, world](const glm::vec2& position) { return GroundAt(terrain, world, position); }, settings.planner),
		random(seed)
	{
		engine.addSystem(lunar::SystemPhase::eFixedUpdate, [this](lunar::Scene& scene, const lunar::FrameTime& frame_time) { fixedUpdate(scene, frame_time); });
		engine.addSystem(lunar::SystemPhase::eUpdate,      [this](lunar::Scene& scene, const lunar::FrameTime& frame_time) { update(scene, frame_time); });
	}

	void Traffic::setTowns(std::vector<Settlement> known_towns)
	{
		towns = std::move(known_towns);
	}

	void Traffic::fixedUpdate(lunar::Scene& scene, const lunar::FrameTime& frame_time)
	{
		const std::shared_ptr<const RoadNetwork> network = roads.getNetwork();
		if (network == nullptr)
			return;

		perceive(scene, *network);
		drive(scene, frame_time.deltaTime);
	}

	void Traffic::update(lunar::Scene& scene, const lunar::FrameTime& frame_time)
	{
		const std::shared_ptr<const RoadNetwork> network = roads.getNetwork();
		if (network == nullptr)
			return;

		populate(scene, *network);
		navigate(scene, *network);
		replan(scene, *network, frame_time.deltaTime);
		present(scene, frame_time.fixedAlpha);
	}

	void Traffic::perceive(lunar::Scene& scene, const RoadNetwork& network) const
	{
		std::vector<Body> bodies;
		BodyGrid          grid;
		scene.forEach<TrafficBody, lunar::Physics::RigidBody>([&](lunar::Entity entity, const TrafficBody& body, lunar::Physics::RigidBody& rigid_body) {
			const glm::vec3 position = lunar::Physics::ToGlm(rigid_body.getBody().getTransform().getPosition());
			grid[BodyCellAt(Flat(position))].push_back(bodies.size());
			bodies.push_back({ .entity = entity, .position = position, .velocity = lunar::Physics::ToGlm(rigid_body.getBody().getLinearVelocity()), .halfLength = body.halfLength });
		});

		const float tolerance = network.getRoadClass().laneWidth * HALF;
		scene.forEach<DrivePath, Leader, TrafficBody>([&](lunar::Entity entity, const DrivePath& path, Leader& leader, const TrafficBody& body) {
			leader = FindLeader(path, entity, body.halfLength, bodies, grid, tolerance, settings.driver.scanDistance);
		});
	}

	void Traffic::drive(lunar::Scene& scene, float delta_time) const
	{
		scene.forEach<TrafficCar, DrivePath, Leader>([&](lunar::Entity, TrafficCar& car, DrivePath& path, const Leader& leader) {
			path.lostFor = path.points.size() < 2 ? path.lostFor + delta_time : 0.f;
			car.vehicle->drive(Decide(*car.vehicle, path, leader, settings.driver), delta_time);
		});
	}

	void Traffic::populate(lunar::Scene& scene, const RoadNetwork& network)
	{
		std::vector<glm::vec3> foci;
		scene.forEach<TrafficFocus, lunar::Transform>([&](lunar::Entity, const TrafficFocus&, const lunar::Transform& transform) {
			foci.push_back(transform.position);
		});

		if (foci.empty())
			return;

		size_t kept = 0;
		scene.forEach<TrafficCar, DrivePath>([&](lunar::Entity entity, const TrafficCar& car, const DrivePath& path) {
			const float distance = DistanceToNearest(foci, car.vehicle->getTransform().position);
			const bool  lost     = path.lostFor > settings.lostTimeout;
			if (distance > settings.despawnRadius || (lost && distance > settings.lostDespawnRadius))
				scene.destroyEntity(entity);
			else
				kept++;
		});

		if (kept < settings.vehicleCount)
			spawn(scene, network, foci[std::uniform_int_distribution<size_t>(0, foci.size() - 1)(random)]);
	}

	void Traffic::spawn(lunar::Scene& scene, const RoadNetwork& network, const glm::vec3& focus)
	{
		std::uniform_real_distribution<float> heading(0.f, glm::two_pi<float>());
		std::uniform_real_distribution<float> reach(0.f, settings.spawnRadius);
		std::bernoulli_distribution           direction;

		const float                    angle = heading(random);
		const glm::vec2                probe = Flat(focus) + glm::vec2(std::cos(angle), std::sin(angle)) * reach(random);
		const std::optional<RoadPoint> road  = network.closestPoint(probe, settings.roadSearch);
		if (!road.has_value())
			return;

		const bool      forward  = direction(random);
		const glm::vec3 position = LanePoint(network, road->road, forward, road->distance) + UP * SPAWN_HEIGHT;
		if (isCrowded(scene, position))
			return;

		std::optional<Trip> trip = planTrip(network, { .road = network.getRoadId(road->road), .forward = forward }, road->distance);
		if (!trip.has_value())
			return;

		auto vehicle = std::make_unique<Vehicle>(scene, engine.getRenderer().getMeshes(), definition, position, IDENTITY);
		vehicle->recover(position, forward ? road->direction : -road->direction);

		const lunar::Entity entity = vehicle->getEntity();
		scene.addComponent<Trip>(entity, std::move(*trip));
		scene.addComponent<DrivePath>(entity);
		scene.addComponent<Leader>(entity);
		scene.addComponent<TrafficCar>(entity, TrafficCar { .vehicle = std::move(vehicle) });
	}

	void Traffic::navigate(lunar::Scene& scene, const RoadNetwork& network)
	{
		scene.forEach<TrafficCar, Trip, DrivePath>([&](lunar::Entity, const TrafficCar& car, Trip& trip, DrivePath& path) {
			const lunar::Transform& transform = car.vehicle->getTransform();
			while (trip.next < trip.checkpoints.size() && HasPassed(trip, transform.position, settings.checkpointReach))
				trip.next++;

			if (trip.next < trip.checkpoints.size())
				return;

			const std::optional<Trip> next_trip = continueTrip(network, transform);
			trip          = next_trip.value_or(Trip {});
			path.replanIn = 0.f;
			if (!next_trip.has_value())
				path.points.clear();
		});
	}

	void Traffic::replan(lunar::Scene& scene, const RoadNetwork& network, float delta_time)
	{
		size_t budget = settings.plansPerFrame;
		scene.forEach<TrafficCar, Trip, DrivePath>([&](lunar::Entity, const TrafficCar& car, const Trip& trip, DrivePath& path) {
			path.replanIn -= delta_time;
			if (budget == 0 || path.replanIn > 0.f || trip.next >= trip.checkpoints.size())
				return;

			budget--;
			const glm::vec3 position = car.vehicle->getTransform().position;
			const size_t    target   = PlanTarget(trip, position, settings.planReach);
			SetCourse(path, planner.plan(network, position, trip.checkpoints[target]), std::span(trip.checkpoints).subspan(target + 1), network.getRoadClass().speedLimit, settings);
		});
	}

	void Traffic::present(lunar::Scene& scene, float alpha)
	{
		const bool debug = engine.isDebugMode();
		scene.forEach<TrafficCar, Trip, DrivePath>([&](lunar::Entity, TrafficCar& car, const Trip& trip, const DrivePath& path) {
			car.vehicle->setSimulated(colliders.isReady(car.vehicle->getTransform().position));
			car.vehicle->updateWheels(alpha);
			if (!debug)
				return;

			DrawLifted(engine.getRenderer(), path.points, PATH_COLOR);
			DrawLifted(engine.getRenderer(), std::span(trip.checkpoints).subspan(std::min(trip.next, trip.checkpoints.size())), ROUTE_COLOR);
		});
	}

	std::optional<Trip> Traffic::continueTrip(const RoadNetwork& network, const lunar::Transform& transform)
	{
		const std::optional<RoadPoint> road = network.closestPoint(Flat(transform.position), settings.roadSearch);
		if (!road.has_value())
			return std::nullopt;

		const bool                forward = glm::dot(road->direction, Flat(transform.rotation * CHASSIS_FORWARD)) >= 0.f;
		const RoadId              id      = network.getRoadId(road->road);
		const std::optional<Trip> ahead   = planTrip(network, { .road = id, .forward = forward }, road->distance);
		return ahead.has_value() ? ahead : planTrip(network, { .road = id, .forward = !forward }, road->distance);
	}

	std::optional<Trip> Traffic::planTrip(const RoadNetwork& network, const RouteStep& from, float start_distance)
	{
		const std::optional<size_t> road        = network.findRoad(from.road);
		const std::optional<size_t> destination = road.has_value() ? pickDestination(network, network.pointAlong(*road, start_distance)) : std::nullopt;
		if (!destination.has_value())
			return std::nullopt;

		const std::optional<std::vector<RouteStep>> route = FindRoute(network, from, *destination);
		if (!route.has_value())
			return std::nullopt;

		return Trip { .checkpoints = RouteCheckpoints(network, *route, start_distance, settings.checkpointSpacing) };
	}

	std::optional<size_t> Traffic::pickDestination(const RoadNetwork& network, const glm::vec3& origin)
	{
		const auto in_reach = [&](const glm::vec3& point) {
			const float distance = glm::distance(Flat(point), Flat(origin));
			return distance >= settings.minimumTrip && distance <= settings.destinationRadius;
		};

		std::vector<size_t> candidates;
		if (std::bernoulli_distribution(settings.townChance)(random))
		{
			for (const Settlement& town : towns)
			{
				const glm::vec3             centre = { static_cast<float>(town.center.x), 0.f, static_cast<float>(town.center.y) };
				const std::optional<size_t> node   = in_reach(centre) ? NearestNode(network, centre, static_cast<float>(town.radius)) : std::nullopt;
				if (node.has_value())
					candidates.push_back(*node);
			}
		}

		if (candidates.empty())
		{
			for (size_t node = 0; node < network.getNodes().size(); node++)
				if (in_reach(network.getNodes()[node].position))
					candidates.push_back(node);
		}

		if (candidates.empty())
			return std::nullopt;

		return candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(random)];
	}

	bool Traffic::isCrowded(lunar::Scene& scene, const glm::vec3& position) const
	{
		bool crowded = false;
		scene.forEach<TrafficBody, lunar::Transform>([&](lunar::Entity, const TrafficBody&, const lunar::Transform& transform) {
			crowded = crowded || glm::distance(transform.position, position) < settings.spawnSpacing;
		});

		return crowded;
	}
}
