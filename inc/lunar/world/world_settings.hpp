#pragma once
#include <lunar/api.hpp>
#include <lunar/world/grid.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lunar::World
{
	struct LUNAR_API WorldSettings
	{
		uint32_t chunkQuads        = 32;
		float    vertexSpacing     = 2.f;
		uint32_t regionChunks      = 128;
		int32_t  sampleReachChunks = 1;
		int32_t  viewRadius        = 12;
		int32_t  regionLoadRadius  = 1;
		int32_t  unloadMargin      = 1;
		size_t   maxJobsInFlight   = 16;

		float getChunkSize() const;
	};

	LUNAR_API double                   SampleCoordinate(int32_t chunk, int32_t sample, const WorldSettings& settings);
	LUNAR_API ChunkCoord               ChunkAt(double x, double z, const WorldSettings& settings);
	LUNAR_API ChunkCoord               ChunkAt(const glm::vec3& position, const WorldSettings& settings);
	LUNAR_API RegionCoord              RegionAt(ChunkCoord chunk, const WorldSettings& settings);
	LUNAR_API RegionCoord              RegionAt(const glm::vec3& position, const WorldSettings& settings);
	LUNAR_API ChunkCoord               ChunkWithinRegion(ChunkCoord chunk, const WorldSettings& settings);
	LUNAR_API glm::vec3                ChunkOrigin(ChunkCoord coord, const WorldSettings& settings);
	LUNAR_API std::vector<RegionCoord> RegionsNeededForChunk(ChunkCoord chunk, const WorldSettings& settings);
}
