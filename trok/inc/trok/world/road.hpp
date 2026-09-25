#pragma once
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain_shaping.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
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
		float       limitPenalty     = 40.f;
		float       turnPenalty      = 0.15f;
		float       climbCost        = 1.f;
		float       cutCost          = 0.3f;
		float       fillCost         = 0.3f;
		float       bridgeCost       = 3.f;
		float       bridgeHeight     = 5.f;
		float       maxCutDepth      = 12.f;
		float       cutSpread        = 1.5f;
		float       gradingMargin    = 3.f;
		float       pillarSpacing    = 30.f;
		float       pillarWidth      = 2.f;
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
		RoadNetwork(std::vector<glm::vec3> centreline, RoadClass road_class)                noexcept;
		RoadNetwork(std::vector<std::vector<glm::vec3>> centrelines, RoadClass road_class) noexcept;

		std::vector<lunar::World::ShapeDeclaration> shapesReaching(const glm::vec2& minimum, const glm::vec2& maximum) const;
		std::vector<uint32_t>      segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		glm::vec3                  sideAt(size_t point)                                   const;
		float                      distanceAt(size_t point)                               const;
		std::span<const glm::vec3> getCentreline()                                        const;
		std::span<const glm::vec3> getRoad(size_t road)                                   const;
		size_t                     getRoadCount()                                         const;
		const RoadClass&           getRoadClass()                                         const;

	private:
		using Cell = lunar::World::GridCoord<struct RoadCellTag>;

		std::vector<uint32_t> candidatesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		bool                  isRoadStart(size_t point)                                         const;
		bool                  isRoadEnd(size_t point)                                           const;

		std::vector<glm::vec3>                                            points;
		std::vector<float>                                                distances;
		std::vector<uint32_t>                                             roadOf;
		std::vector<uint32_t>                                             roadStarts;
		RoadClass                                                         roadClass;
		std::unordered_map<Cell, std::vector<uint32_t>, lunar::World::GridCoordHash> cells;
	};

	struct RoadLink
	{
		glm::vec2 from = {};
		glm::vec2 to   = {};
	};

	class RoadLayer
	{
	public:
		void                               set(std::shared_ptr<const RoadNetwork> network);
		std::shared_ptr<const RoadNetwork> get() const;

	private:
		mutable std::mutex                 mutex;
		std::shared_ptr<const RoadNetwork> network;
	};
}
