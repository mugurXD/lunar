#pragma once
#include <lunar/world/region.hpp>
#include <lunar/world/terrain_generator.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>

struct TestPlan
{
	static constexpr int32_t REGION_VALUE_SCALE = 1000;

	int32_t value = 0;

	bool operator==(const TestPlan&) const = default;

	static nlohmann::json Serialize(const TestPlan& plan)
	{
		return { { "value", plan.value } };
	}

	static std::optional<TestPlan> Deserialize(const nlohmann::json& json)
	{
		const int32_t value = json.at("value").get<int32_t>();
		if (value < 0)
			return std::nullopt;

		return TestPlan { value };
	}

	static int32_t ValueFor(lunar::World::RegionCoord coord)
	{
		return std::abs(coord.x) * REGION_VALUE_SCALE + std::abs(coord.z);
	}
};

class TestPlanner final : public lunar::World::RegionPlanner<TestPlan>
{
public:
	TestPlan plan(lunar::World::RegionCoord coord, const lunar::World::WorldSettings&) const override
	{
		planCount++;
		return TestPlan { TestPlan::ValueFor(coord) };
	}

	TestPlan restore(TestPlan loaded, lunar::World::RegionCoord, const lunar::World::WorldSettings&) const override
	{
		restoreCount++;
		return loaded;
	}

	mutable std::atomic<int> planCount    = 0;
	mutable std::atomic<int> restoreCount = 0;
};

class WaveGenerator final : public lunar::World::TerrainGenerator<TestPlan>
{
public:
	static constexpr double WAVE_FREQUENCY = 0.05;
	static constexpr double WAVE_AMPLITUDE = 10.0;

	float sampleHeight(const lunar::World::RegionContext<TestPlan>&, double x, double z) const override
	{
		sampleCount++;
		return static_cast<float>(std::sin(x * WAVE_FREQUENCY) * WAVE_AMPLITUDE + std::cos(z * WAVE_FREQUENCY) * WAVE_AMPLITUDE);
	}

	glm::vec3 sampleColor(const lunar::World::RegionContext<TestPlan>&, double, double, float, const glm::vec3& normal) const override
	{
		return normal;
	}

	mutable std::atomic<int> sampleCount = 0;
};
