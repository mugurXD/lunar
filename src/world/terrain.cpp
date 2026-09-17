#include <lunar/world/terrain.hpp>

namespace lunar::World
{
	namespace
	{
		constexpr uint32_t BORDER_SIDES        = 2;
		constexpr float    CENTRAL_DIFFERENCES = 2.f;
		constexpr size_t   INDICES_PER_QUAD    = 6;
	}

	float Heightmap::sample(int32_t x, int32_t z) const
	{
		const size_t row    = static_cast<size_t>(z + HEIGHTMAP_BORDER);
		const size_t column = static_cast<size_t>(x + HEIGHTMAP_BORDER);
		return heights[row * samplesPerSide + column];
	}

	uint32_t HeightmapSamplesPerSide(const WorldSettings& settings)
	{
		return settings.chunkQuads + 1 + BORDER_SIDES * HEIGHTMAP_BORDER;
	}

	glm::vec3 HeightmapNormal(const Heightmap& heightmap, int32_t x, int32_t z, const WorldSettings& settings)
	{
		const float slope_x = heightmap.sample(x + 1, z) - heightmap.sample(x - 1, z);
		const float slope_z = heightmap.sample(x, z + 1) - heightmap.sample(x, z - 1);

		return glm::normalize(glm::vec3(-slope_x, CENTRAL_DIFFERENCES * settings.vertexSpacing, -slope_z));
	}

	void AppendChunkIndices(Render::MeshData& mesh, const WorldSettings& settings)
	{
		const uint32_t vertices_per_side = settings.chunkQuads + 1;
		mesh.indices.reserve(mesh.indices.size() + static_cast<size_t>(settings.chunkQuads) * settings.chunkQuads * INDICES_PER_QUAD);

		for (uint32_t z = 0; z < settings.chunkQuads; z++)
		{
			for (uint32_t x = 0; x < settings.chunkQuads; x++)
			{
				const uint32_t top_left     = z * vertices_per_side + x;
				const uint32_t top_right    = top_left + 1;
				const uint32_t bottom_left  = top_left + vertices_per_side;
				const uint32_t bottom_right = bottom_left + 1;

				mesh.indices.insert(mesh.indices.end(), { top_left, bottom_left, top_right, top_right, bottom_left, bottom_right });
			}
		}
	}
}
