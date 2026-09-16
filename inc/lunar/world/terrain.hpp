#pragma once
#include <lunar/api.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lunar::World
{
	struct LUNAR_API ChunkCoord
	{
		int32_t x = 0;
		int32_t z = 0;

		bool operator==(const ChunkCoord&) const = default;
	};

	struct LUNAR_API ChunkCoordHash
	{
		size_t operator()(const ChunkCoord& coord) const;
	};

	struct LUNAR_API TerrainSettings
	{
		uint32_t chunkQuads      = 32;
		float    vertexSpacing   = 2.f;
		int32_t  viewRadius      = 12;
		int32_t  unloadMargin    = 1;
		size_t   maxJobsInFlight = 16;

		float getChunkSize() const;
	};

	struct LUNAR_API TerrainChunk
	{
		ChunkCoord coord = {};
	};

	class LUNAR_API TerrainGenerator
	{
	public:
		TerrainGenerator()          noexcept = default;
		virtual ~TerrainGenerator() noexcept = default;

		TerrainGenerator(const TerrainGenerator&)            = delete;
		TerrainGenerator& operator=(const TerrainGenerator&) = delete;

		virtual float     sampleHeight(double x, double z)                                         const = 0;
		virtual glm::vec3 sampleColor(double x, double z, float height, const glm::vec3& normal) const = 0;
	};

	struct LUNAR_API Heightmap
	{
		uint32_t           samplesPerSide = 0;
		std::vector<float> heights        = {};

		float sample(int32_t x, int32_t z) const;
	};

	struct LUNAR_API ChunkData
	{
		Heightmap        heightmap = {};
		Render::MeshData mesh      = {};
	};

	LUNAR_API ChunkCoord ChunkAt(const glm::vec3& position, const TerrainSettings& settings);
	LUNAR_API glm::vec3  ChunkOrigin(ChunkCoord coord, const TerrainSettings& settings);
	LUNAR_API ChunkData  GenerateChunk(const TerrainGenerator& generator, ChunkCoord coord, const TerrainSettings& settings);
}
