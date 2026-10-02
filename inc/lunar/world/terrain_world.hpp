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
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace lunar::World
{
	class LUNAR_API TerrainWorld
	{
	public:
		TerrainWorld(Scene&                 scene,
		             JobSystem&             jobs,
		             Render::MeshRegistry&  meshes,
		             ChunkSource&           source,
		             const WorldSettings&   settings,
		             Render::MaterialHandle material = {}) noexcept;
		~TerrainWorld() noexcept;

		TerrainWorld(const TerrainWorld&)            = delete;
		TerrainWorld& operator=(const TerrainWorld&) = delete;

		void   update(const glm::vec3& focus_position);
		void   reload();
		void   refresh(std::span<const ChunkCoord> coords);
		size_t           getLoadedChunkCount()          const;
		size_t           getPendingChunkCount()         const;
		const Heightmap*             findHeightmap(ChunkCoord coord) const;
		std::span<const DressedMesh> findSolids(ChunkCoord coord)    const;
		uint64_t                     getRevision(ChunkCoord coord)   const;

	private:
		struct LoadedChunk
		{
			JobHandle                       job            = {};
			GameObject                      object         = nullptr;
			Render::MeshHandle              mesh           = {};
			std::vector<Render::MeshHandle> dressingMeshes = {};
			std::vector<DressedMesh>        solids         = {};
			Heightmap                       heightmap      = {};
			uint64_t                        revision       = 0;
		};

		void unloadDistantChunks(ChunkCoord center);
		void requestMissingChunks(ChunkCoord center);
		void      requestChunk(ChunkCoord coord, ChunkWork work);
		JobHandle submitChunk(ChunkCoord coord, ChunkWork work);
		void      onChunkGenerated(ChunkCoord coord, ChunkData data);
		void      unloadChunk(const LoadedChunk& chunk);
		void      destroyResources(const LoadedChunk& chunk);

		Scene&                                                     scene;
		JobSystem&                                                 jobs;
		Render::MeshRegistry&                                      meshes;
		ChunkSource&                                               source;
		WorldSettings                                              settings;
		Render::MaterialHandle                                     material;
		std::unordered_map<ChunkCoord, LoadedChunk, GridCoordHash> chunks;
		size_t                                                     pendingCount = 0;
		uint64_t                                                   nextRevision = 1;
	};
}
