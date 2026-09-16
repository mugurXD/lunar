#pragma once
#include <lunar/api.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>

class FastNoiseLite;

namespace lunar::World
{
	struct LUNAR_API NoiseTerrainSettings
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

	class LUNAR_API NoiseTerrainGenerator final : public TerrainGenerator
	{
	public:
		NoiseTerrainGenerator(const NoiseTerrainSettings& settings) noexcept;
		~NoiseTerrainGenerator() noexcept override;

		float     sampleHeight(double x, double z)                                         const override;
		glm::vec3 sampleColor(double x, double z, float height, const glm::vec3& normal) const override;

	private:
		NoiseTerrainSettings           settings;
		std::unique_ptr<FastNoiseLite> noise;
	};
}
