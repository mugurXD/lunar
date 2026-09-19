#pragma once
#include <lunar/api.hpp>
#include <lunar/core/jobs.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <unordered_map>

namespace lunar::World
{
	class LUNAR_API TerrainWorld
	{
	public:
		TerrainWorld(Scene&                scene,
		             JobSystem&            jobs,
		             Render::MeshRegistry& meshes,
		             ChunkSource&          source,
		             const WorldSettings&  settings) noexcept;
		~TerrainWorld() noexcept;

		TerrainWorld(const TerrainWorld&)            = delete;
		TerrainWorld& operator=(const TerrainWorld&) = delete;

		void   update(const glm::vec3& focus_position);
		size_t           getLoadedChunkCount()          const;
		size_t           getPendingChunkCount()         const;
		const Heightmap* findHeightmap(ChunkCoord coord) const;

	private:
		struct LoadedChunk
		{
			JobHandle          job       = {};
			GameObject         object    = nullptr;
			Render::MeshHandle mesh      = {};
			Heightmap          heightmap = {};
		};

		void unloadDistantChunks(ChunkCoord center);
		void requestMissingChunks(ChunkCoord center);
		void requestChunk(ChunkCoord coord, ChunkWork work);
		void onChunkGenerated(ChunkCoord coord, ChunkData data);
		void unloadChunk(const LoadedChunk& chunk);

		Scene&                                                     scene;
		JobSystem&                                                 jobs;
		Render::MeshRegistry&                                      meshes;
		ChunkSource&                                               source;
		WorldSettings                                              settings;
		std::unordered_map<ChunkCoord, LoadedChunk, GridCoordHash> chunks;
		size_t                                                     pendingCount = 0;
	};
}
