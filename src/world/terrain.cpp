#include <lunar/world/terrain.hpp>
#include <lunar/utils/hash.hpp>

#include <algorithm>
#include <array>
#include <span>

namespace lunar::World
{
	namespace
	{
		constexpr uint32_t BORDER_SIDES        = 2;
		constexpr float    CENTRAL_DIFFERENCES = 2.f;
		constexpr size_t   INDICES_PER_QUAD    = 6;
		constexpr size_t   GRID_COPIES         = 2;
		constexpr uint32_t FIRST_COPY          = 0;
		constexpr float    TRIANGLE_CORNERS    = 3.f;
		constexpr uint64_t DIAGONAL_BIT        = 1;
		constexpr int      UNIT_BITS           = 24;
		constexpr int      UNIT_SHIFT          = 64 - UNIT_BITS;
		constexpr float    UNIT_SCALE          = 1.f / static_cast<float>(1u << UNIT_BITS);
		constexpr float    SIGNED_RANGE        = 2.f;

		using Triangle = std::array<uint32_t, 3>;

		uint64_t CellHash(ChunkCoord coord, uint32_t x, uint32_t z, const WorldSettings& settings)
		{
			const int64_t cell_x = static_cast<int64_t>(coord.x) * settings.chunkQuads + x;
			const int64_t cell_z = static_cast<int64_t>(coord.z) * settings.chunkQuads + z;
			return MixHash(MixHash(0, static_cast<uint64_t>(cell_x)), static_cast<uint64_t>(cell_z));
		}

		float SignedUnit(uint64_t hash)
		{
			return static_cast<float>(hash >> UNIT_SHIFT) * UNIT_SCALE * SIGNED_RANGE - 1.f;
		}

		void AppendFacet(Render::MeshData& mesh, std::span<const glm::vec4> grid_colors, uint32_t copy, const Triangle& corners, float brightness)
		{
			const auto& [first, second, third] = corners;
			const uint32_t  provoking          = first + copy;
			const glm::vec4 average            = (grid_colors[first] + grid_colors[second] + grid_colors[third]) / TRIANGLE_CORNERS;

			mesh.vertices[provoking].color = glm::vec4(glm::vec3(average) * brightness, average.a);
			mesh.indices.insert(mesh.indices.end(), { provoking, second, third });
		}
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

	void FacetChunkMesh(Render::MeshData& mesh, ChunkCoord coord, const WorldSettings& settings)
	{
		const uint32_t vertices_per_side = settings.chunkQuads + 1;
		const uint32_t grid_size         = static_cast<uint32_t>(mesh.vertices.size());

		std::vector<glm::vec4> grid_colors(grid_size);
		std::ranges::transform(mesh.vertices, grid_colors.begin(), &Render::Vertex::color);

		mesh.vertices.reserve(grid_size * GRID_COPIES);
		for (uint32_t index = 0; index < grid_size; index++)
			mesh.vertices.push_back(mesh.vertices[index]);

		mesh.indices.reserve(mesh.indices.size() + static_cast<size_t>(settings.chunkQuads) * settings.chunkQuads * INDICES_PER_QUAD);

		for (uint32_t z = 0; z < settings.chunkQuads; z++)
		{
			for (uint32_t x = 0; x < settings.chunkQuads; x++)
			{
				const uint32_t top_left     = z * vertices_per_side + x;
				const uint32_t top_right    = top_left + 1;
				const uint32_t bottom_left  = top_left + vertices_per_side;
				const uint32_t bottom_right = bottom_left + 1;

				const uint64_t cell_hash   = CellHash(coord, x, z, settings);

				const float    first_tone  = 1.f + settings.facetVariation * SignedUnit(MixHash(cell_hash, FIRST_COPY));
				const float    second_tone = 1.f + settings.facetVariation * SignedUnit(MixHash(cell_hash, grid_size));

				if ((cell_hash & DIAGONAL_BIT) != 0)
				{
					AppendFacet(mesh, grid_colors, FIRST_COPY, { top_left, bottom_left, bottom_right }, first_tone);
					AppendFacet(mesh, grid_colors, grid_size,  { top_right, top_left, bottom_right },   second_tone);
					continue;
				}

				AppendFacet(mesh, grid_colors, FIRST_COPY, { top_left, bottom_left, top_right },     first_tone);
				AppendFacet(mesh, grid_colors, grid_size,  { top_right, bottom_left, bottom_right }, second_tone);
			}
		}
	}
}
