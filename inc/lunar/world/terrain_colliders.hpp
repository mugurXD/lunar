#pragma once
#include <lunar/api.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain_world.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>

namespace lunar
{
	class Scene;
}

namespace lunar::World
{
	class LUNAR_API TerrainColliders
	{
	public:
		TerrainColliders(Scene& scene, const TerrainWorld& terrain, const WorldSettings& settings, int32_t radius_chunks) noexcept;

		TerrainColliders(const TerrainColliders&)            = delete;
		TerrainColliders& operator=(const TerrainColliders&) = delete;

		void   update(std::span<const glm::vec3> focus_positions);
		bool   isReady(const glm::vec3& position) const;
		size_t getColliderCount()                 const;

	private:
		bool isNeeded(ChunkCoord coord, std::span<const ChunkCoord> focus_chunks) const;
		void create(ChunkCoord coord, const Heightmap& heightmap);

		Scene&                                                            scene;
		const TerrainWorld&                                               terrain;
		WorldSettings                                                     settings;
		int32_t                                                           radiusChunks = 0;
		std::unordered_map<ChunkCoord, Physics::RigidBody, GridCoordHash> colliders;
	};
}
