#pragma once
#include <lunar/api.hpp>
#include <lunar/core/jobs.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/world/terrain.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace lunar::World
{
	class LUNAR_API TerrainWorld
	{
	public:
		TerrainWorld(Scene&                                  scene,
		             JobSystem&                              jobs,
		             Render::MeshRegistry&                   meshes,
		             std::shared_ptr<const TerrainGenerator> generator,
		             const TerrainSettings&                  settings) noexcept;
		~TerrainWorld() noexcept;

		TerrainWorld(const TerrainWorld&)            = delete;
		TerrainWorld& operator=(const TerrainWorld&) = delete;

		void   update(const glm::vec3& focus_position);
		size_t getLoadedChunkCount()  const;
		size_t getPendingChunkCount() const;

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
		void requestChunk(ChunkCoord coord);
		void onChunkGenerated(ChunkCoord coord, ChunkData data);
		void unloadChunk(const LoadedChunk& chunk);

		Scene&                                                      scene;
		JobSystem&                                                  jobs;
		Render::MeshRegistry&                                       meshes;
		std::shared_ptr<const TerrainGenerator>                     generator;
		TerrainSettings                                             settings;
		std::unordered_map<ChunkCoord, LoadedChunk, ChunkCoordHash> chunks;
		size_t                                                      pendingCount = 0;
	};
}
