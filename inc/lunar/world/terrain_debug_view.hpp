#pragma once
#include <lunar/api.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/world/terrain_world.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_map>

namespace lunar
{
	class Scene;
}

namespace lunar::World
{
	class LUNAR_API TerrainDebugView
	{
	public:
		TerrainDebugView(Scene& scene, Render::MeshRegistry& meshes, const TerrainWorld& terrain, const WorldSettings& settings, int32_t radius_chunks) noexcept;
		~TerrainDebugView() noexcept;

		TerrainDebugView(const TerrainDebugView&)            = delete;
		TerrainDebugView& operator=(const TerrainDebugView&) = delete;

		void update(const glm::vec3& focus);
		void clear();

	private:
		struct Overlay
		{
			GameObject         object    = {};
			Render::MeshHandle mesh      = {};
			Heightmap          heightmap = {};
		};

		void create(ChunkCoord coord, const Heightmap& heightmap);
		void destroy(Overlay& overlay);

		Scene&                                                  scene;
		Render::MeshRegistry&                                   meshes;
		const TerrainWorld&                                     terrain;
		WorldSettings                                           settings;
		int32_t                                                 radiusChunks = 0;
		std::unordered_map<ChunkCoord, Overlay, GridCoordHash> overlays;
	};
}
