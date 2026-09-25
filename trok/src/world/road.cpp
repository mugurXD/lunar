#include <trok/world/road.hpp>
#include <trok/json_math.hpp>

#include <lunar/file/json_file.hpp>
#include <lunar/utils/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr uint32_t ROAD_FORMAT_VERSION = 2;
		constexpr float    CELL_SIZE           = 64.f;
		constexpr float    HALF                = 0.5f;
		constexpr float    MIN_MITRE_SCALE     = 0.5f;

		lunar::World::GridCoord<struct RoadCellTag> CellAt(float x, float z)
		{
			return { static_cast<int32_t>(std::floor(x / CELL_SIZE)), static_cast<int32_t>(std::floor(z / CELL_SIZE)) };
		}

		glm::vec3 Perpendicular(const glm::vec3& from, const glm::vec3& to)
		{
			const glm::vec2 direction = glm::normalize(glm::vec2(to.x - from.x, to.z - from.z));
			return { -direction.y, 0.f, direction.x };
		}

		float FlatWidth(const RoadClass& road_class)
		{
			return road_class.halfWidth() + road_class.gradingMargin;
		}

		float GradingReach(const RoadClass& road_class)
		{
			return FlatWidth(road_class) + road_class.maxCutDepth * road_class.cutSpread;
		}

		bool Reaches(const glm::vec3& from, const glm::vec3& to, const glm::vec2& minimum, const glm::vec2& maximum, float reach)
		{
			return std::max(from.x, to.x) + reach >= minimum.x && std::min(from.x, to.x) - reach <= maximum.x &&
			       std::max(from.z, to.z) + reach >= minimum.y && std::min(from.z, to.z) - reach <= maximum.y;
		}

		lunar::World::ShapePoint ShapePointOf(const glm::vec3& point, float core)
		{
			return { .position = { point.x, point.z }, .height = point.y, .core = core };
		}
	}

	float RoadClass::halfWidth() const
	{
		return static_cast<float>(lanes) * laneWidth * HALF + shoulderWidth;
	}

	nlohmann::json RoadClass::Serialize(const RoadClass& road_class)
	{
		return nlohmann::json
		{
			{ "formatVersion",   ROAD_FORMAT_VERSION },
			{ "name",            road_class.name },
			{ "lanes",           road_class.lanes },
			{ "laneWidth",       road_class.laneWidth },
			{ "shoulderWidth",   road_class.shoulderWidth },
			{ "minCurveRadius",  road_class.minCurveRadius },
			{ "maxGrade",        road_class.maxGrade },
			{ "limitPenalty",    road_class.limitPenalty },
			{ "turnPenalty",     road_class.turnPenalty },
			{ "climbCost",       road_class.climbCost },
			{ "cutCost",         road_class.cutCost },
			{ "fillCost",        road_class.fillCost },
			{ "bridgeCost",      road_class.bridgeCost },
			{ "bridgeHeight",    road_class.bridgeHeight },
			{ "maxCutDepth",     road_class.maxCutDepth },
			{ "cutSpread",       road_class.cutSpread },
			{ "gradingMargin",   road_class.gradingMargin },
			{ "pillarSpacing",   road_class.pillarSpacing },
			{ "pillarWidth",     road_class.pillarWidth },
			{ "surfaceOffset",   road_class.surfaceOffset },
			{ "edgeDepth",       road_class.edgeDepth },
			{ "color",           SerializeVec3(road_class.color) }
		};
	}

	std::optional<RoadClass> RoadClass::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != ROAD_FORMAT_VERSION)
			return std::nullopt;

		const RoadClass road_class =
		{
			.name            = json.at("name").get<std::string>(),
			.lanes           = json.at("lanes").get<int32_t>(),
			.laneWidth       = json.at("laneWidth").get<float>(),
			.shoulderWidth   = json.at("shoulderWidth").get<float>(),
			.minCurveRadius  = json.at("minCurveRadius").get<float>(),
			.maxGrade        = json.at("maxGrade").get<float>(),
			.limitPenalty    = json.at("limitPenalty").get<float>(),
			.turnPenalty     = json.at("turnPenalty").get<float>(),
			.climbCost       = json.at("climbCost").get<float>(),
			.cutCost         = json.at("cutCost").get<float>(),
			.fillCost        = json.at("fillCost").get<float>(),
			.bridgeCost      = json.at("bridgeCost").get<float>(),
			.bridgeHeight    = json.at("bridgeHeight").get<float>(),
			.maxCutDepth     = json.at("maxCutDepth").get<float>(),
			.cutSpread       = json.at("cutSpread").get<float>(),
			.gradingMargin   = json.at("gradingMargin").get<float>(),
			.pillarSpacing   = json.at("pillarSpacing").get<float>(),
			.pillarWidth     = json.at("pillarWidth").get<float>(),
			.surfaceOffset   = json.at("surfaceOffset").get<float>(),
			.edgeDepth       = json.at("edgeDepth").get<float>(),
			.color           = DeserializeVec3(json.at("color"))
		};

		if (road_class.lanes <= 0 || road_class.laneWidth <= 0.f || road_class.maxGrade <= 0.f || road_class.minCurveRadius <= 0.f ||
		    road_class.pillarSpacing <= 0.f || road_class.maxCutDepth <= 0.f || road_class.cutSpread < 0.f)
		{
			Fs::ReportMalformedJson("a road class needs positive lanes, lane width, grade, curve radius, cut depth and pillar spacing, and a non-negative cut spread");
			return std::nullopt;
		}

		return road_class;
	}

	RoadNetwork::RoadNetwork(std::vector<glm::vec3> centreline, RoadClass road_class) noexcept
		: RoadNetwork(std::vector<std::vector<glm::vec3>> { std::move(centreline) }, std::move(road_class))
	{
	}

	RoadNetwork::RoadNetwork(std::vector<std::vector<glm::vec3>> centrelines, RoadClass road_class) noexcept
		: roadClass(std::move(road_class))
	{
		for (const std::vector<glm::vec3>& road : centrelines)
		{
			roadStarts.push_back(static_cast<uint32_t>(points.size()));
			for (const glm::vec3& point : road)
			{
				distances.push_back(points.size() == roadStarts.back() ? 0.f : distances.back() + glm::distance(points.back(), point));
				roadOf.push_back(static_cast<uint32_t>(roadStarts.size() - 1));
				points.push_back(point);
			}
		}

		roadStarts.push_back(static_cast<uint32_t>(points.size()));

		const float reach = GradingReach(roadClass);

		for (uint32_t segment = 0; segment + 1 < points.size(); segment++)
		{
			if (isRoadEnd(segment))
				continue;

			const glm::vec3& from    = points[segment];
			const glm::vec3& to      = points[segment + 1];
			const Cell       minimum = CellAt(std::min(from.x, to.x) - reach, std::min(from.z, to.z) - reach);
			const Cell       maximum = CellAt(std::max(from.x, to.x) + reach, std::max(from.z, to.z) + reach);

			for (int32_t z = minimum.z; z <= maximum.z; z++)
				for (int32_t x = minimum.x; x <= maximum.x; x++)
					cells[{ x, z }].push_back(segment);
		}
	}

	bool RoadNetwork::isRoadStart(size_t point) const
	{
		return point == roadStarts[roadOf[point]];
	}

	bool RoadNetwork::isRoadEnd(size_t point) const
	{
		return point + 1 == roadStarts[roadOf[point] + 1];
	}

	std::span<const glm::vec3> RoadNetwork::getRoad(size_t road) const
	{
		return std::span(points).subspan(roadStarts[road], roadStarts[road + 1] - roadStarts[road]);
	}

	size_t RoadNetwork::getRoadCount() const
	{
		return roadStarts.empty() ? 0 : roadStarts.size() - 1;
	}

	std::vector<lunar::World::ShapeDeclaration> RoadNetwork::shapesReaching(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		const float core  = FlatWidth(roadClass);
		const float reach = GradingReach(roadClass);

		std::vector<lunar::World::ShapeDeclaration> shapes;
		uint32_t                                    previous = 0;

		for (const uint32_t segment : candidatesWithin(minimum, maximum))
		{
			const glm::vec3& from = points[segment];
			const glm::vec3& to   = points[segment + 1];
			if (!Reaches(from, to, minimum, maximum, reach))
				continue;

			const bool continues = !shapes.empty() && segment == previous + 1;
			if (!continues)
				shapes.push_back({ .points = { ShapePointOf(from, core) }, .spread = roadClass.cutSpread, .reach = reach });

			shapes.back().points.push_back(ShapePointOf(to, core));
			previous = segment;
		}

		return shapes;
	}

	glm::vec3 RoadNetwork::sideAt(size_t point) const
	{
		const bool      first  = isRoadStart(point);
		const bool      last   = isRoadEnd(point);
		const glm::vec3 before = Perpendicular(points[first ? point : point - 1], points[first ? point + 1 : point]);
		const glm::vec3 after  = Perpendicular(points[last ? point - 1 : point], points[last ? point : point + 1]);
		const glm::vec3 mitre  = glm::normalize(before + after);

		return mitre * (roadClass.halfWidth() / std::max(glm::dot(mitre, after), MIN_MITRE_SCALE));
	}

	float RoadNetwork::distanceAt(size_t point) const
	{
		return distances[point];
	}

	std::vector<uint32_t> RoadNetwork::segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		std::vector<uint32_t> found = candidatesWithin(minimum, maximum);

		std::erase_if(found, [&](uint32_t segment) {
			const glm::vec3 middle = (points[segment] + points[segment + 1]) * HALF;
			return middle.x < minimum.x || middle.x >= maximum.x || middle.z < minimum.y || middle.z >= maximum.y;
		});

		return found;
	}

	std::vector<uint32_t> RoadNetwork::candidatesWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
	{
		const Cell lowest  = CellAt(minimum.x, minimum.y);
		const Cell highest = CellAt(maximum.x, maximum.y);

		std::vector<uint32_t> found;

		for (int32_t z = lowest.z; z <= highest.z; z++)
		{
			for (int32_t x = lowest.x; x <= highest.x; x++)
			{
				const auto cell = cells.find({ x, z });
				if (cell == cells.end())
					continue;

				found.insert(found.end(), cell->second.begin(), cell->second.end());
			}
		}

		std::ranges::sort(found);
		const auto duplicates = std::ranges::unique(found);
		found.erase(duplicates.begin(), duplicates.end());
		return found;
	}

	std::span<const glm::vec3> RoadNetwork::getCentreline() const
	{
		return points;
	}

	const RoadClass& RoadNetwork::getRoadClass() const
	{
		return roadClass;
	}

	void RoadLayer::set(std::shared_ptr<const RoadNetwork> replacement)
	{
		const std::lock_guard lock(mutex);
		network = std::move(replacement);
	}

	std::shared_ptr<const RoadNetwork> RoadLayer::get() const
	{
		const std::lock_guard lock(mutex);
		return network;
	}
}
