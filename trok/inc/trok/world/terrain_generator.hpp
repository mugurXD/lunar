#pragma once
#include <trok/world/region_plan.hpp>
#include <lunar/world/terrain_generator.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>

class FastNoiseLite;

namespace trok
{
	struct TerrainSettings
	{
		int32_t   seed           = 1337;
		float     frequency      = 0.0025f;
		int32_t   octaves        = 5;
		float     amplitude      = 60.f;
		glm::vec3 lowGrassColor  = { 0.10f, 0.22f, 0.06f };
		glm::vec3 highGrassColor = { 0.22f, 0.36f, 0.10f };
		glm::vec3 rockColor      = { 0.32f, 0.30f, 0.28f };
		float     rockSlopeStart = 0.25f;
		float     rockSlopeEnd   = 0.45f;
	};

	class TerrainGenerator final : public lunar::World::TerrainGenerator<RegionPlan>
	{
	public:
		TerrainGenerator(const TerrainSettings& settings) noexcept;
		~TerrainGenerator() noexcept override;

		float     sampleHeight(const RegionContext& context, double x, double z)                                         const override;
		glm::vec3 sampleColor(const RegionContext& context, double x, double z, float height, const glm::vec3& normal) const override;

	private:
		TerrainSettings                settings;
		std::unique_ptr<FastNoiseLite> noise;
	};
}
