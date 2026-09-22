#include <trok/world/road.hpp>
#include <trok/geometry.hpp>
#include <trok/json_math.hpp>

#include <lunar/file/json_file.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr uint32_t ROAD_FORMAT_VERSION = 1;
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
			return std::max(road_class.maxGradingWidth, FlatWidth(road_class));
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
			{ "slopePenalty",    road_class.slopePenalty },
			{ "turnPenalty",     road_class.turnPenalty },
			{ "embankmentSpread", road_class.embankmentSpread },
			{ "maxFillHeight",   road_class.maxFillHeight },
			{ "maxGradingWidth", road_class.maxGradingWidth },
			{ "gradingMargin",   road_class.gradingMargin },
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
			.slopePenalty    = json.at("slopePenalty").get<float>(),
			.turnPenalty     = json.at("turnPenalty").get<float>(),
			.embankmentSpread = json.at("embankmentSpread").get<float>(),
			.maxGradingWidth = json.at("maxGradingWidth").get<float>(),
			.gradingMargin   = json.at("gradingMargin").get<float>(),
			.maxFillHeight   = json.at("maxFillHeight").get<float>(),
			.surfaceOffset   = json.at("surfaceOffset").get<float>(),
			.edgeDepth       = json.at("edgeDepth").get<float>(),
			.color           = DeserializeVec3(json.at("color"))
		};

		if (road_class.lanes <= 0 || road_class.laneWidth <= 0.f || road_class.maxGrade <= 0.f || road_class.embankmentSpread < 0.f || road_class.maxFillHeight <= 0.f)
		{
			Fs::ReportMalformedJson("a road class needs positive lanes, lane width, grade and fill height, and a non-negative embankment spread");
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

	float RoadNetwork::gradedHeight(double x, double z, float terrain_height) const
	{
		const Nearest nearest = nearestPoint(x, z);
		if (!nearest.found || terrain_height <= nearest.height)
			return terrain_height;

		const float flat_width = FlatWidth(roadClass);
		if (nearest.distance <= flat_width)
			return nearest.height;

		const float cut           = terrain_height - nearest.height;
		const float grading_width = std::min(flat_width + cut * roadClass.embankmentSpread, GradingReach(roadClass));
		if (nearest.distance >= grading_width)
			return terrain_height;

		const float amount = (nearest.distance - flat_width) / (grading_width - flat_width);
		return glm::mix(nearest.height, terrain_height, glm::smoothstep(0.f, 1.f, amount));
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

	std::vector<uint32_t> RoadNetwork::segmentsWithin(const glm::vec2& minimum, const glm::vec2& maximum) const
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

				for (const uint32_t segment : cell->second)
				{
					const glm::vec3 middle = (points[segment] + points[segment + 1]) * HALF;
					if (middle.x >= minimum.x && middle.x < maximum.x && middle.z >= minimum.y && middle.z < maximum.y)
						found.push_back(segment);
				}
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

	RoadNetwork::Nearest RoadNetwork::nearestPoint(double x, double z) const
	{
		const auto found = cells.find(CellAt(static_cast<float>(x), static_cast<float>(z)));
		if (found == cells.end())
			return {};

		const glm::vec2 point = { static_cast<float>(x), static_cast<float>(z) };
		Nearest         nearest = { .distance = GradingReach(roadClass) };

		for (const uint32_t segment : found->second)
		{
			const glm::vec3& from     = points[segment];
			const glm::vec3& to       = points[segment + 1];
			float            amount   = 0.f;
			const float      distance = DistanceToSegment(point, { from.x, from.z }, { to.x, to.z }, amount);

			if (distance >= nearest.distance)
				continue;

			nearest = { .distance = distance, .height = glm::mix(from.y, to.y, amount), .found = true };
		}

		return nearest;
	}
}
