#pragma once
#include <lunar/api.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstdint>

namespace lunar
{
	class Scene;
}

namespace lunar::World
{
	LUNAR_API ChunkCoord SceneChunkAt(const Scene& scene, const glm::vec3& position, const WorldSettings& settings);
	LUNAR_API glm::vec3  SceneChunkOrigin(const Scene& scene, ChunkCoord coord, const WorldSettings& settings);
	LUNAR_API bool       RecenterOrigin(Scene& scene, const glm::vec3& focus, const WorldSettings& settings, int32_t radius_chunks);
}
