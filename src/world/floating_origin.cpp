#include <lunar/world/floating_origin.hpp>
#include <lunar/core/scene.hpp>

namespace lunar::World
{
	namespace
	{
		glm::dvec3 ChunkCorner(ChunkCoord coord, const WorldSettings& settings)
		{
			return { SampleCoordinate(coord.x, 0, settings), 0.0, SampleCoordinate(coord.z, 0, settings) };
		}
	}

	ChunkCoord SceneChunkAt(const Scene& scene, const glm::vec3& position, const WorldSettings& settings)
	{
		const glm::dvec3 absolute = scene.toAbsolute(position);
		return ChunkAt(absolute.x, absolute.z, settings);
	}

	glm::vec3 SceneChunkOrigin(const Scene& scene, ChunkCoord coord, const WorldSettings& settings)
	{
		return scene.toScene(ChunkCorner(coord, settings));
	}

	bool RecenterOrigin(Scene& scene, const glm::vec3& focus, const WorldSettings& settings, int32_t radius_chunks)
	{
		const ChunkCoord offset = ChunkAt(focus, settings);
		if (IsInsideSquare(offset, ChunkCoord {}, radius_chunks))
			return false;

		// Whole-chunk steps keep every chunk origin exact in scene space, so chunk-local meshes and colliders stay valid as they are moved.
		scene.shiftOrigin(ChunkCorner(offset, settings));
		return true;
	}
}
