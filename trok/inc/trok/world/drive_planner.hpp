#pragma once
#include <trok/world/road.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace trok
{
	using GroundSampler = std::function<std::optional<float>(const glm::vec2& position)>;

	struct DrivePlannerSettings
	{
		float    seaLevel        = -std::numeric_limits<float>::infinity();
		float    cellSize        = 1.f;
		float    windowMargin    = 24.f;
		int32_t  maxWindowCells  = 256;
		float    laneCost        = 4.f;
		float    offRoadCost     = 8.f;
		float    gradeCost       = 20.f;
		float    maxGrade        = 0.6f;
		float    pathSpacing     = 2.f;
		uint32_t smoothingPasses = 4;
	};

	struct DrivePoint
	{
		glm::vec3 position = {};
		bool      onRoad   = false;
	};

	class DrivePlanner
	{
	public:
		DrivePlanner(GroundSampler ground, DrivePlannerSettings settings = {}) noexcept;

		std::vector<DrivePoint> plan(const RoadNetwork& network, const glm::vec3& from, const glm::vec3& to);

	private:
		enum class Surface : uint8_t
		{
			eUnknown,
			eBlocked,
			eTerrain,
			eRoad,
			eJunction
		};

		struct Cell
		{
			Surface   surface      = Surface::eUnknown;
			float     height       = 0.f;
			float     offset       = 0.f;
			glm::vec2 direction    = {};
			float     roadDistance = std::numeric_limits<float>::infinity();
		};

		bool                    prepare(const RoadNetwork& network, const glm::vec2& from, const glm::vec2& to);
		void                    stampRoads(const RoadNetwork& network, const glm::vec2& minimum, const glm::vec2& maximum);
		void                    stampJunctions(const RoadNetwork& network, const glm::vec2& minimum, const glm::vec2& maximum);
		const Cell&             resolve(uint32_t index);
		float                   stepCost(const Cell& from, const Cell& to, const glm::vec2& move, float length, const RoadClass& road_class) const;
		std::vector<DrivePoint> trace(uint32_t goal, const glm::vec3& from, const glm::vec3& to)                                       const;
		std::vector<DrivePoint> smooth(const std::vector<DrivePoint>& path)                                                             const;
		glm::ivec2              cellOf(const glm::vec2& position)                                                                       const;
		glm::vec2               centreOf(uint32_t index)                                                                                const;

		GroundSampler         ground;
		DrivePlannerSettings  settings;
		glm::vec2             origin  = {};
		glm::ivec2            size    = {};
		std::vector<Cell>     cells   = {};
		std::vector<float>    costs   = {};
		std::vector<uint32_t> parents = {};
	};
}
