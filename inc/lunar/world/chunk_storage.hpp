#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/world/world_settings.hpp>

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace lunar::World
{
	class LUNAR_API ChunkStorage
	{
	public:
		ChunkStorage(const Fs::Path& directory, const WorldSettings& settings) noexcept;

		ChunkStorage(const ChunkStorage&)            = delete;
		ChunkStorage& operator=(const ChunkStorage&) = delete;

		std::optional<Heightmap> load(ChunkCoord coord)                             const;
		bool                     save(ChunkCoord coord, const Heightmap& heightmap) const;

	private:
		struct RegionIndex
		{
			bool                                                    usable   = true;
			uint64_t                                                validEnd = 0;
			std::unordered_map<ChunkCoord, uint64_t, GridCoordHash> offsets  = {};
		};

		RegionIndex& getIndex(RegionCoord region)            const;
		RegionIndex  scanRegion(RegionCoord region)          const;
		bool         createRegionFile(const Fs::Path& path)  const;
		Fs::Path     getRegionPath(RegionCoord region)       const;
		size_t       getSampleCount()                        const;
		uint64_t     getRecordSize()                         const;

		Fs::Path                                                            directory;
		WorldSettings                                                       settings;
		mutable std::mutex                                                  mutex;
		mutable std::unordered_map<RegionCoord, RegionIndex, GridCoordHash> indices;
	};
}
