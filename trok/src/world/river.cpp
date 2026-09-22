#include <trok/world/river.hpp>
#include <trok/geometry.hpp>
#include <trok/json_math.hpp>
#include <trok/world/stable_hash.hpp>

#include <lunar/file/json_file.hpp>

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr double HALF                      = 0.5;
		constexpr int    RIVER_MAX_STEPS           = 130;
		constexpr double RIVER_STEP                = 125.0;
		constexpr double RIVER_GRADIENT_EPSILON    = 125.0;
		constexpr double RIVER_MIN_SLOPE           = 0.0005;
		constexpr double RIVER_INERTIA             = 0.55;
		constexpr double RIVER_MIN_WIDTH           = 10.0;
		constexpr double RIVER_MAX_WIDTH           = 70.0;
		constexpr double RIVER_FULL_WIDTH_DISTANCE = 12000.0;
		constexpr double RIVER_DEPTH_RATIO         = 0.08;
		constexpr double RIVER_BANK_SPREAD         = 2.0;
		constexpr double RIVER_VALLEY_RATIO        = 14.0;
		constexpr double RIVER_VALLEY_MAX_WIDTH    = 600.0;
		constexpr size_t RIVER_POOL_POINTS         = 5;
		constexpr double RIVER_POOL_SCALE          = 2.2;
		constexpr size_t RIVER_MIN_POINTS         = 8;
		constexpr double RIVER_SOURCE_JITTER      = 0.5;

		double UnitFromHash(uint64_t hash)
		{
			return static_cast<double>(hash >> 11) * 0x1p-53;
		}

		nlohmann::json SerializePoint(const RiverPoint& point)
		{
			return nlohmann::json { { "position", { point.position.x, point.position.y } }, { "width", point.width }, { "bed", point.bed } };
		}

		RiverPoint DeserializePoint(const nlohmann::json& json)
		{
			return RiverPoint
			{
				.position = { json.at("position").at(0).get<double>(), json.at("position").at(1).get<double>() },
				.width    = json.at("width").get<double>(),
				.bed      = json.at("bed").get<double>()
			};
		}
	}

	double RiverDepth(double width)
	{
		return width * RIVER_DEPTH_RATIO;
	}

	double RiverReach()
	{
		return RIVER_VALLEY_MAX_WIDTH;
	}

	double RiverValleyWidth(double width)
	{
		return std::min(width * RIVER_VALLEY_RATIO, RIVER_VALLEY_MAX_WIDTH);
	}

	double RiverMaxLength()
	{
		return RIVER_MAX_STEPS * RIVER_STEP;
	}

	glm::dvec2 RiverSource(int32_t seed, int32_t block_x, int32_t block_z, double block_size)
	{
		const uint64_t roll = MixHash(MixHash(static_cast<uint64_t>(seed) ^ GOLDEN_GAMMA, static_cast<uint64_t>(block_x)), static_cast<uint64_t>(block_z));
		const glm::dvec2 offset = { UnitFromHash(roll) - HALF, UnitFromHash(MixHash(roll, 1)) - HALF };

		return (glm::dvec2(block_x, block_z) + glm::dvec2(HALF) + offset * RIVER_SOURCE_JITTER) * block_size;
	}

	nlohmann::json River::Serialize(const River& river)
	{
		nlohmann::json points = nlohmann::json::array();
		for (const RiverPoint& point : river.points)
			points.push_back(SerializePoint(point));

		return nlohmann::json
		{
			{ "id",      river.id },
			{ "minimum", { river.minimum.x, river.minimum.y } },
			{ "maximum", { river.maximum.x, river.maximum.y } },
			{ "points",  std::move(points) }
		};
	}

	std::optional<River> River::Deserialize(const nlohmann::json& json)
	{
		River river =
		{
			.id      = json.at("id").get<uint32_t>(),
			.minimum = { json.at("minimum").at(0).get<double>(), json.at("minimum").at(1).get<double>() },
			.maximum = { json.at("maximum").at(0).get<double>(), json.at("maximum").at(1).get<double>() }
		};

		for (const nlohmann::json& point : json.at("points"))
			river.points.push_back(DeserializePoint(point));

		return river;
	}

	River TraceRiver(uint32_t id, const glm::dvec2& source, const ElevationCurve& elevation, const ClimateSampler& climate, const BiomeLibrary& biomes)
	{
		const auto height_at = [&](const glm::dvec2& at) {
			const Climate sample = climate.sample(at.x, at.y);
			return static_cast<double>(elevation.heightAt(sample.continentalness) + biomes.heightOffsetAt(sample));
		};

		River      river     = { .id = id, .minimum = source, .maximum = source };
		glm::dvec2 position  = source;
		glm::dvec2 heading   = {};
		double     travelled = 0.0;

		for (int step = 0; step < RIVER_MAX_STEPS; step++)
		{
			const double     here     = height_at(position);
			const glm::dvec2 gradient =
			{
				(height_at(position + glm::dvec2(RIVER_GRADIENT_EPSILON, 0.0)) - here) / RIVER_GRADIENT_EPSILON,
				(height_at(position + glm::dvec2(0.0, RIVER_GRADIENT_EPSILON)) - here) / RIVER_GRADIENT_EPSILON
			};

			const double slope = glm::length(gradient);
			if (slope < RIVER_MIN_SLOPE || here <= elevation.seaLevel)
				break;

			const glm::dvec2 downhill = -gradient / slope;
			heading = step == 0 ? downhill : glm::normalize(glm::mix(downhill, heading, RIVER_INERTIA));

			const double width = glm::mix(RIVER_MIN_WIDTH, RIVER_MAX_WIDTH, std::min(travelled / RIVER_FULL_WIDTH_DISTANCE, 1.0));
			const double sunk  = std::max(here - RiverDepth(width), static_cast<double>(elevation.seaLevel));
			const double bed   = river.points.empty() ? sunk : std::min(sunk, river.points.back().bed);

			river.points.push_back({ .position = position, .width = width, .bed = bed });
			river.minimum = glm::min(river.minimum, position);
			river.maximum = glm::max(river.maximum, position);

			position  += heading * RIVER_STEP;
			travelled += RIVER_STEP;
		}

		if (river.points.size() < RIVER_MIN_POINTS)
		{
			river.points.clear();
			return river;
		}

		for (size_t back = 0; back < RIVER_POOL_POINTS; back++)
		{
			RiverPoint& point = river.points[river.points.size() - 1 - back];
			const double amount = static_cast<double>(RIVER_POOL_POINTS - back) / RIVER_POOL_POINTS;

			point.width *= glm::mix(1.0, RIVER_POOL_SCALE, amount);
		}

		return river;
	}

	River ClipRiver(const River& river, const glm::dvec2& minimum, const glm::dvec2& maximum)
	{
		River clipped = { .id = river.id };

		for (size_t index = 0; index < river.points.size(); index++)
		{
			const glm::dvec2& at = river.points[index].position;
			const bool inside = at.x >= minimum.x && at.x <= maximum.x && at.y >= minimum.y && at.y <= maximum.y;

			const bool neighbour_inside =
				(index > 0 && river.points[index - 1].position.x >= minimum.x && river.points[index - 1].position.x <= maximum.x
				           && river.points[index - 1].position.y >= minimum.y && river.points[index - 1].position.y <= maximum.y) ||
				(index + 1 < river.points.size() && river.points[index + 1].position.x >= minimum.x && river.points[index + 1].position.x <= maximum.x
				                                 && river.points[index + 1].position.y >= minimum.y && river.points[index + 1].position.y <= maximum.y);

			if (!inside && !neighbour_inside)
				continue;

			clipped.points.push_back(river.points[index]);
			clipped.minimum = clipped.points.size() == 1 ? at : glm::min(clipped.minimum, at);
			clipped.maximum = clipped.points.size() == 1 ? at : glm::max(clipped.maximum, at);
		}

		return clipped;
	}

	float CarvedHeight(const std::vector<River>& rivers, double x, double z, float terrain_height)
	{
		const glm::dvec2 point   = { x, z };
		const double     reach   = RiverReach();
		float            carved  = terrain_height;

		for (const River& river : rivers)
		{
			if (point.x < river.minimum.x - reach || point.x > river.maximum.x + reach ||
			    point.y < river.minimum.y - reach || point.y > river.maximum.y + reach)
				continue;

			for (size_t index = 0; index + 1 < river.points.size(); index++)
			{
				const RiverPoint& from  = river.points[index];
				const RiverPoint& to    = river.points[index + 1];
				const double      widest = RiverValleyWidth(std::max(from.width, to.width));

				if (point.x < std::min(from.position.x, to.position.x) - widest || point.x > std::max(from.position.x, to.position.x) + widest ||
				    point.y < std::min(from.position.y, to.position.y) - widest || point.y > std::max(from.position.y, to.position.y) + widest)
					continue;

				double       amount   = 0.0;
				const double distance = DistanceToSegment(point, from.position, to.position, amount);
				const double width    = glm::mix(from.width, to.width, amount);
				const double basin    = RiverValleyWidth(width);
				if (distance >= basin)
					continue;

				const double bed     = glm::mix(from.bed, to.bed, amount);
				const double depth   = RiverDepth(width);
				const double surface = bed + depth;
				const double valley  = glm::mix(surface, static_cast<double>(terrain_height), glm::smoothstep(0.0, 1.0, distance / basin));

				const double waterline = width * HALF;
				if (distance >= waterline)
				{
					carved = std::min(carved, static_cast<float>(valley));
					continue;
				}

				const double floor   = std::max(waterline - depth * RIVER_BANK_SPREAD, 0.0);
				const double bank    = waterline - floor;
				const double across  = bank > 0.0 ? std::clamp((distance - floor) / bank, 0.0, 1.0) : 0.0;
				const double channel = glm::mix(bed, surface, glm::smoothstep(0.0, 1.0, across));

				carved = std::min(carved, static_cast<float>(std::min(channel, valley)));
			}
		}

		return carved;
	}
}
