#pragma once
#include <lunar/world/grid.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace trok
{
	struct RoadClass
	{
		std::string name             = {};
		int32_t     lanes            = 2;
		float       laneWidth        = 3.5f;
		float       shoulderWidth    = 1.f;
		float       minCurveRadius   = 60.f;
		float       maxGrade         = 0.08f;
		float       slopePenalty     = 40.f;
		float       turnPenalty      = 0.15f;
		float       embankmentSpread = 2.f;
		float       maxGradingWidth  = 30.f;
		float       gradingMargin    = 3.f;
		float       maxFillHeight    = 10.f;
		float       surfaceOffset    = 0.06f;
		float       edgeDepth        = 0.5f;
		glm::vec3   color            = { 0.16f, 0.16f, 0.17f };

		float halfWidth() const;

		bool operator==(const RoadClass&) const = default;

		static nlohmann::json           Serialize(const RoadClass& road_class);
		static std::optional<RoadClass> Deserialize(const nlohmann::json& json);
	};

	class RoadNetwork
	{
	public:
		RoadNetwork(std::vector<glm::vec3> centreline, RoadClass road_class) noexcept;

		float                      gradedHeight(double x, double z, float terrain_height) const;
		glm::vec3                  sideAt(size_t point)                                   const;
		std::vector<uint32_t>      segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		std::span<const glm::vec3> getCentreline()                                        const;
		const RoadClass&           getRoadClass()                                         const;

	private:
		using Cell = lunar::World::GridCoord<struct RoadCellTag>;

		struct Nearest
		{
			float distance = 0.f;
			float height   = 0.f;
			bool  found    = false;
		};

		Nearest nearestPoint(double x, double z) const;

		std::vector<glm::vec3>                                            points;
		RoadClass                                                         roadClass;
		std::unordered_map<Cell, std::vector<uint32_t>, lunar::World::GridCoordHash> cells;
	};
}
