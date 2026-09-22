#include <trok/world/elevation.hpp>

#include <lunar/file/json_file.hpp>

#include <algorithm>
#include <cstddef>

namespace trok
{
	namespace
	{
		constexpr uint32_t ELEVATION_FORMAT_VERSION = 1;
		constexpr size_t   MIN_CURVE_POINTS         = 2;
	}

	float ElevationCurve::heightAt(float continentalness) const
	{
		const auto upper = std::ranges::upper_bound(points, continentalness, {}, &glm::vec2::x);
		if (upper == points.begin())
			return points.front().y;

		if (upper == points.end())
			return points.back().y;

		const glm::vec2 lower  = *(upper - 1);
		const float     amount = (continentalness - lower.x) / (upper->x - lower.x);
		return glm::mix(lower.y, upper->y, amount);
	}

	nlohmann::json ElevationCurve::Serialize(const ElevationCurve& curve)
	{
		nlohmann::json points = nlohmann::json::array();
		for (const glm::vec2& point : curve.points)
			points.push_back({ point.x, point.y });

		return nlohmann::json
		{
			{ "formatVersion", ELEVATION_FORMAT_VERSION },
			{ "points",        std::move(points) },
			{ "seaLevel",      curve.seaLevel }
		};
	}

	std::optional<ElevationCurve> ElevationCurve::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != ELEVATION_FORMAT_VERSION)
			return std::nullopt;

		ElevationCurve curve = { .points = {}, .seaLevel = json.at("seaLevel").get<float>() };
		for (const nlohmann::json& point : json.at("points"))
			curve.points.emplace_back(point.at(0).get<float>(), point.at(1).get<float>());

		const auto not_increasing = [](const glm::vec2& left, const glm::vec2& right) { return left.x >= right.x; };
		if (curve.points.size() < MIN_CURVE_POINTS || std::ranges::adjacent_find(curve.points, not_increasing) != curve.points.end())
		{
			Fs::ReportMalformedJson("an elevation curve needs at least two points with increasing continentalness");
			return std::nullopt;
		}

		return curve;
	}
}
