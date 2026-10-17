#include <trok/vehicle/traffic_spawning.hpp>
#include <trok/world/road_routes.hpp>

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr float HALF                 = 0.5f;
		constexpr float METRES_PER_KILOMETRE = 1000.f;

		glm::vec2 Flat(const glm::vec3& point)
		{
			return { point.x, point.z };
		}
	}

	bool IsHidden(const TrafficViewer& viewer, const glm::vec3& point)
	{
		const glm::vec3 offset   = point - viewer.position;
		const float     distance = glm::length(offset);
		if (distance < viewer.nearDistance)
			return false;

		if (distance > viewer.hiddenDistance)
			return true;

		return glm::dot(offset / distance, viewer.forward) < std::cos(glm::radians(viewer.viewAngle));
	}

	std::vector<uint32_t> SegmentsAround(const RoadNetwork& network, const glm::vec3& centre, float inner_radius, float outer_radius)
	{
		const std::span<const glm::vec3> points = network.getCentreline();
		const glm::vec2                  middle = Flat(centre);

		std::vector<uint32_t> segments = network.segmentsWithin(middle - outer_radius, middle + outer_radius);
		std::erase_if(segments, [&](uint32_t segment) {
			const float distance = glm::distance((Flat(points[segment]) + Flat(points[segment + 1])) * HALF, middle);
			return distance < inner_radius || distance > outer_radius;
		});

		return segments;
	}

	float LaneKilometresNear(const RoadNetwork& network, const glm::vec3& centre, float radius)
	{
		const std::span<const glm::vec3> points = network.getCentreline();

		float metres = 0.f;
		for (const uint32_t segment : SegmentsAround(network, centre, 0.f, radius))
			metres += glm::distance(Flat(points[segment]), Flat(points[segment + 1]));

		return metres * static_cast<float>(network.getRoadClass().lanes) / METRES_PER_KILOMETRE;
	}

	size_t VehiclesNear(std::span<const LaneVehicle> vehicles, const glm::vec3& centre, float radius)
	{
		return static_cast<size_t>(std::ranges::count_if(vehicles, [&](const LaneVehicle& vehicle) { return glm::distance(Flat(vehicle.position), Flat(centre)) <= radius; }));
	}

	LaneGaps GapsAround(const RoadNetwork& network, const LaneSpot& spot, std::span<const LaneVehicle> vehicles, float half_length)
	{
		const RoadClass& road_class = network.getRoadClass();
		const float      direction  = spot.forward ? 1.f : -1.f;
		const float      lane_side  = LaneOffset(road_class, spot.forward);

		LaneGaps gaps;
		for (const LaneVehicle& vehicle : vehicles)
		{
			const std::optional<RoadPoint> road = network.closestPoint(Flat(vehicle.position), road_class.halfWidth());
			if (!road.has_value() || road->road != spot.road)
				continue;

			const bool same_lane = glm::dot(vehicle.position - road->position, network.sideAlong(road->road, road->distance)) * lane_side > 0.f;
			if (!same_lane)
				continue;

			const float along = (road->distance - spot.distance) * direction;
			const float gap   = std::abs(along) - half_length - vehicle.halfLength;
			const float speed = glm::dot(vehicle.velocity, glm::vec3(road->direction.x, 0.f, road->direction.y)) * direction;
			if (along >= 0.f && gap < gaps.ahead)
			{
				gaps.ahead      = gap;
				gaps.aheadSpeed = speed;
			}
			else if (along < 0.f && gap < gaps.behind)
			{
				gaps.behind      = gap;
				gaps.behindSpeed = speed;
			}
		}

		return gaps;
	}

	std::optional<float> SpawnSpeed(const LaneGaps& gaps, float cruise_speed, const DriverSettings& settings)
	{
		const float leader_speed   = std::max(gaps.aheadSpeed, 0.f);
		const float follower_speed = std::max(gaps.behindSpeed, 0.f);
		const float speed          = gaps.ahead >= DesiredGap(cruise_speed, cruise_speed - leader_speed, settings) ? cruise_speed : std::min(cruise_speed, leader_speed);
		const bool  room_ahead     = gaps.ahead >= DesiredGap(speed, speed - leader_speed, settings);
		const bool  room_behind    = gaps.behind >= DesiredGap(follower_speed, follower_speed - speed, settings);

		return room_ahead && room_behind ? std::optional<float>(speed) : std::nullopt;
	}
}
