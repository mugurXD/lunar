#pragma once
#include <trok/vehicle/car_following.hpp>
#include <trok/world/road.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <span>

namespace trok
{
	using TrafficDensity = std::function<float(const glm::vec3& position)>;

	struct TrafficViewer
	{
		glm::vec3 position       = {};
		glm::vec3 forward        = { 0.f, 0.f, -1.f };
		float     nearDistance   = 0.f;
		float     hiddenDistance = std::numeric_limits<float>::infinity();
		float     viewAngle      = 0.f;
	};

	struct LaneVehicle
	{
		glm::vec3 position   = {};
		glm::vec3 velocity   = {};
		float     halfLength = 0.f;
	};

	struct LaneSpot
	{
		size_t road     = 0;
		bool   forward  = true;
		float  distance = 0.f;
	};

	struct LaneGaps
	{
		float ahead       = std::numeric_limits<float>::infinity();
		float aheadSpeed  = 0.f;
		float behind      = std::numeric_limits<float>::infinity();
		float behindSpeed = 0.f;
	};

	bool                 IsHidden(const TrafficViewer& viewer, const glm::vec3& point);
	float                LaneKilometresNear(const RoadNetwork& network, const glm::vec3& centre, float radius);
	size_t               VehiclesNear(std::span<const LaneVehicle> vehicles, const glm::vec3& centre, float radius);
	LaneGaps             GapsAround(const RoadNetwork& network, const LaneSpot& spot, std::span<const LaneVehicle> vehicles, float half_length);
	std::optional<float> SpawnSpeed(const LaneGaps& gaps, float cruise_speed, const DriverSettings& settings);
}
