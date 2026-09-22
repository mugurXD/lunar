#pragma once
#include <trok/world/biome.hpp>
#include <trok/world/climate.hpp>
#include <trok/world/elevation.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace trok
{
	constexpr double RIVER_SOURCE_REGIONS = 2.0;

	struct RiverPoint
	{
		glm::dvec2 position = {};
		double     width    = 0.0;
		double     bed      = 0.0;

		bool operator==(const RiverPoint&) const = default;
	};

	struct River
	{
		uint32_t                id      = 0;
		glm::dvec2              minimum = {};
		glm::dvec2              maximum = {};
		std::vector<RiverPoint> points  = {};

		bool operator==(const River&) const = default;

		static nlohmann::json   Serialize(const River& river);
		static std::optional<River> Deserialize(const nlohmann::json& json);
	};

	double     RiverDepth(double width);
	double     RiverReach();
	double     RiverValleyWidth(double width);
	double     RiverMaxLength();
	glm::dvec2 RiverSource(int32_t seed, int32_t block_x, int32_t block_z, double block_size);

	River TraceRiver(uint32_t id, const glm::dvec2& source, const ElevationCurve& elevation, const ClimateSampler& climate, const BiomeLibrary& biomes);
	River ClipRiver(const River& river, const glm::dvec2& minimum, const glm::dvec2& maximum);

	float CarvedHeight(const std::vector<River>& rivers, double x, double z, float terrain_height);
}
