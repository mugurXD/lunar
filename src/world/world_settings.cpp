#include <lunar/world/world_settings.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace lunar::World
{
	namespace
	{
		constexpr int COORD_HALF_BITS = 32;
	}

	size_t HashGridCoord(int32_t x, int32_t z)
	{
		const uint64_t packed = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << COORD_HALF_BITS) | static_cast<uint32_t>(z);
		return std::hash<uint64_t> {}(packed);
	}

	float WorldSettings::getChunkSize() const
	{
		return static_cast<float>(chunkQuads) * vertexSpacing;
	}

	double SampleCoordinate(int32_t chunk, int32_t sample, const WorldSettings& settings)
	{
		const int64_t sample_index = static_cast<int64_t>(chunk) * settings.chunkQuads + sample;
		return static_cast<double>(sample_index) * settings.vertexSpacing;
	}

	ChunkCoord ChunkAt(double x, double z, const WorldSettings& settings)
	{
		const double chunk_size = settings.getChunkSize();

		return ChunkCoord
		{
			.x = static_cast<int32_t>(std::floor(x / chunk_size)),
			.z = static_cast<int32_t>(std::floor(z / chunk_size))
		};
	}

	ChunkCoord ChunkAt(const glm::vec3& position, const WorldSettings& settings)
	{
		return ChunkAt(position.x, position.z, settings);
	}

	RegionCoord RegionAt(ChunkCoord chunk, const WorldSettings& settings)
	{
		const auto region_chunks = static_cast<int32_t>(settings.regionChunks);

		return RegionCoord
		{
			.x = FloorDivide(chunk.x, region_chunks),
			.z = FloorDivide(chunk.z, region_chunks)
		};
	}

	RegionCoord RegionAt(const glm::vec3& position, const WorldSettings& settings)
	{
		return RegionAt(ChunkAt(position, settings), settings);
	}

	ChunkCoord ChunkWithinRegion(ChunkCoord chunk, const WorldSettings& settings)
	{
		const auto        region_chunks = static_cast<int32_t>(settings.regionChunks);
		const RegionCoord region        = RegionAt(chunk, settings);

		return ChunkCoord
		{
			.x = chunk.x - region.x * region_chunks,
			.z = chunk.z - region.z * region_chunks
		};
	}

	glm::vec3 ChunkOrigin(ChunkCoord coord, const WorldSettings& settings)
	{
		return glm::vec3(static_cast<float>(SampleCoordinate(coord.x, 0, settings)), 0.f, static_cast<float>(SampleCoordinate(coord.z, 0, settings)));
	}

	std::vector<RegionCoord> RegionsNeededForChunk(ChunkCoord chunk, const WorldSettings& settings)
	{
		std::vector<RegionCoord> needed;

		for (int32_t offset_z = -settings.sampleReachChunks; offset_z <= settings.sampleReachChunks; offset_z++)
		{
			for (int32_t offset_x = -settings.sampleReachChunks; offset_x <= settings.sampleReachChunks; offset_x++)
			{
				const RegionCoord region = RegionAt(ChunkCoord { chunk.x + offset_x, chunk.z + offset_z }, settings);
				if (std::ranges::find(needed, region) == needed.end())
					needed.push_back(region);
			}
		}

		return needed;
	}
}
