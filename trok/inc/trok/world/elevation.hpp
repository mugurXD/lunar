#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <vector>

namespace trok
{
	struct ElevationCurve
	{
		std::vector<glm::vec2> points = { { -1.f, 0.f }, { 1.f, 0.f } };

		float heightAt(float continentalness) const;

		bool operator==(const ElevationCurve&) const = default;

		static nlohmann::json                Serialize(const ElevationCurve& curve);
		static std::optional<ElevationCurve> Deserialize(const nlohmann::json& json);
	};
}
