#pragma once
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain_shaping.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace trok
{
	struct RoadFurniture
	{
		float     markingWidth  = 0.15f;
		float     dashLength    = 3.f;
		float     dashGap       = 6.f;
		glm::vec3 markingColor  = { 0.85f, 0.85f, 0.8f };
		glm::vec3 centreColor   = { 0.95f, 0.72f, 0.15f };
		float     postSpacing   = 50.f;
		float     postHeight    = 1.f;
		float     postWidth     = 0.12f;
		float     postOutset    = 0.5f;
		glm::vec3 postColor     = { 0.9f, 0.9f, 0.9f };
		float     railElevation = 1.5f;
		float     railHeight    = 1.4f;
		float     railDepth     = 1.4f;
		float     railThickness = 0.6f;
		glm::vec3 railColor     = { 0.6f, 0.62f, 0.65f };

		bool operator==(const RoadFurniture&) const = default;
	};

	struct RoadClass
	{
		std::string   name                = {};
		int32_t       lanes               = 2;
		float         laneWidth           = 3.5f;
		float         shoulderWidth       = 1.f;
		float         minCurveRadius      = 60.f;
		float         maxGrade            = 0.08f;
		float         speedLimit          = 25.f;
		float         limitPenalty        = 40.f;
		float         turnPenalty         = 0.15f;
		float         climbCost           = 1.f;
		float         cutCost             = 0.3f;
		float         fillCost            = 0.3f;
		float         bridgeCost          = 3.f;
		float         bridgeHeight        = 5.f;
		float         bridgeHeightPenalty = 0.2f;
		float         maxCutDepth         = 12.f;
		float         cutSpread           = 1.5f;
		float         gradingMargin       = 3.f;
		float         pillarSpacing       = 30.f;
		float         pillarWidth         = 2.f;
		float         surfaceOffset       = 0.06f;
		float         edgeDepth           = 0.5f;
		float         edgeSpread          = 3.f;
		float         maxEdgeRun          = 2.f;
		glm::vec3     color               = { 0.16f, 0.16f, 0.17f };
		RoadFurniture furniture           = {};

		float halfWidth() const;

		bool operator==(const RoadClass&) const = default;

		static nlohmann::json           Serialize(const RoadClass& road_class);
		static std::optional<RoadClass> Deserialize(const nlohmann::json& json);
	};

	using RoadId = uint32_t;

	struct RoadPoint
	{
		glm::vec3 position  = {};
		glm::vec2 direction = {};
		uint32_t  road      = 0;
		float     distance  = 0.f;
	};

	struct RoadEnd
	{
		uint32_t road    = 0;
		bool     atStart = false;
		float    trim    = 0.f;
	};

	struct NodeCorner
	{
		glm::vec3 position  = {};
		bool      opensRoad = false;
	};

	struct RoadNode
	{
		glm::vec3            position = {};
		std::vector<RoadEnd> ends     = {};
		float                radius   = 0.f;
	};

	class RoadNetwork
	{
	public:
		RoadNetwork(std::vector<glm::vec3> centreline, RoadClass road_class)                noexcept;
		RoadNetwork(std::vector<std::vector<glm::vec3>> centrelines, RoadClass road_class, std::vector<RoadId> ids = {}) noexcept;

		std::vector<lunar::World::ShapeDeclaration> shapesReaching(const glm::vec2& minimum, const glm::vec2& maximum) const;
		std::vector<uint32_t>      segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		std::optional<RoadPoint>   closestPoint(const glm::vec2& position, float max_distance, std::optional<size_t> excluded_road = std::nullopt) const;
		glm::vec3                  sideAt(size_t point)                                   const;
		float                      distanceAt(size_t point)                               const;
		std::span<const glm::vec3> getCentreline()                                        const;
		std::span<const glm::vec3> getRoad(size_t road)                                   const;
		size_t                     getRoadCount()                                         const;
		size_t                     getRoadAt(size_t point)                                const;
		RoadId                     getRoadId(size_t road)                                 const;
		std::optional<size_t>      findRoad(RoadId id)                                    const;
		size_t                     getNodeAt(size_t road, bool at_start)                  const;
		float                      lengthOf(size_t road)                                  const;
		glm::vec3                  pointAlong(size_t road, float distance)                const;
		glm::vec3                  sideAlong(size_t road, float distance)                 const;
		float                      trimAt(size_t road, bool at_start)                     const;
		std::span<const RoadNode>  getNodes()                                             const;
		std::vector<uint32_t>      nodesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		std::vector<NodeCorner>    nodeOutline(size_t node)                               const;
		const RoadClass&           getRoadClass()                                         const;

	private:
		using Cell = lunar::World::GridCoord<struct RoadCellTag>;

		std::vector<uint32_t> candidatesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const;
		bool                  isRoadStart(size_t point)                                         const;
		bool                  isRoadEnd(size_t point)                                           const;
		std::pair<size_t, float> locate(size_t road, float distance)                           const;
		glm::vec2             directionOf(const RoadNode& node, const RoadEnd& end)             const;
		void                  computeTrims(RoadNode& node)                                      const;

		std::vector<glm::vec3>                                            points;
		std::vector<float>                                                distances;
		std::vector<uint32_t>                                             roadOf;
		std::vector<uint32_t>                                             roadStarts;
		std::vector<RoadNode>                                             nodes;
		std::vector<std::array<uint32_t, 2>>                              roadNodes;
		std::vector<RoadId>                                               roadIds;
		std::unordered_map<RoadId, uint32_t>                              roadIndices;
		RoadClass                                                         roadClass;
		std::unordered_map<Cell, std::vector<uint32_t>, lunar::World::GridCoordHash> cells;
	};

	struct RoadLink
	{
		glm::vec2 from = {};
		glm::vec2 to   = {};
	};

	struct RoadConnectivity
	{
		std::vector<size_t>                roadGroups = {};
		std::vector<std::optional<size_t>> townGroups = {};
	};

	using TownLocator = std::function<std::optional<size_t>(const glm::vec3& point)>;

	std::vector<glm::vec3> ArriveAt(std::vector<glm::vec3> road, const glm::vec3& node, const RoadClass& road_class);

	RoadConnectivity ConnectRoads(const RoadNetwork& network, size_t town_count, const TownLocator& town_at);

	std::vector<lunar::World::ChunkCoord> ChunksReachedBy(std::span<const glm::vec3> road, const RoadClass& road_class, const lunar::World::WorldSettings& settings);

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
