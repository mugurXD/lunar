#include <lunar/world/terrain.hpp>
#include <lunar/utils/hash.hpp>
#include <lunar/debug.hpp>

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
		constexpr uint64_t CELL_SEED           = 0;
		constexpr uint64_t FACET_SEED          = 1;
		constexpr uint64_t FIRST_HALF          = 0;
		constexpr uint64_t SECOND_HALF         = 1;
		constexpr int      UNIT_BITS           = 24;
		constexpr int      UNIT_SHIFT          = 64 - UNIT_BITS;
		constexpr float    UNIT_SCALE          = 1.f / static_cast<float>(1u << UNIT_BITS);
		constexpr float    SIGNED_RANGE        = 2.f;

		using Triangle = std::array<uint32_t, 3>;
		using Halves   = std::array<Triangle, 2>;

		struct Facet
		{
			glm::vec3 normal = {};
			float     tone   = 1.f;
		};

		struct FacetCell
		{
			uint32_t             left         = 0;
			uint32_t             top          = 0;
			bool                 mainDiagonal = false;
			std::array<Facet, 2> halves       = {};
		};

		uint64_t CellHash(uint64_t seed, int64_t cell_x, int64_t cell_z)
		{
			return MixHash(MixHash(seed, static_cast<uint64_t>(cell_x)), static_cast<uint64_t>(cell_z));
		}

		float SignedUnit(uint64_t hash)
		{
			return static_cast<float>(hash >> UNIT_SHIFT) * UNIT_SCALE * SIGNED_RANGE - 1.f;
		}

		Halves SplitQuad(uint32_t top_left, uint32_t size, uint32_t vertices_per_side, bool main_diagonal)
		{
			const uint32_t top_right    = top_left + size;
			const uint32_t bottom_left  = top_left + size * vertices_per_side;
			const uint32_t bottom_right = bottom_left + size;

			if (main_diagonal)
				return { Triangle { top_left, bottom_left, bottom_right }, Triangle { top_right, top_left, bottom_right } };

			return { Triangle { top_left, bottom_left, top_right }, Triangle { top_right, bottom_left, bottom_right } };
		}

		glm::vec3 PlaneNormal(const Render::MeshData& mesh, const Triangle& corners)
		{
			const glm::vec3& first  = mesh.vertices[corners[0]].position;
			const glm::vec3& second = mesh.vertices[corners[1]].position;
			const glm::vec3& third  = mesh.vertices[corners[2]].position;
			return glm::normalize(glm::cross(second - first, third - first));
		}

		FacetCell MakeFacetCell(const Render::MeshData& mesh, ChunkCoord coord, uint32_t facet_x, uint32_t facet_z, const WorldSettings& settings)
		{
			const uint32_t vertices_per_side = settings.chunkQuads + 1;
			const int64_t  facets_per_chunk  = settings.chunkQuads / settings.facetSize;
			const uint64_t hash              = CellHash(FACET_SEED, coord.x * facets_per_chunk + facet_x, coord.z * facets_per_chunk + facet_z);
			const uint32_t left              = facet_x * settings.facetSize;
			const uint32_t top               = facet_z * settings.facetSize;
			const bool     main_diagonal     = (hash & DIAGONAL_BIT) != 0;
			const Halves   halves            = SplitQuad(top * vertices_per_side + left, settings.facetSize, vertices_per_side, main_diagonal);

			const auto facet = [&](size_t half, uint64_t salt) {
				return Facet { PlaneNormal(mesh, halves[half]), 1.f + settings.facetVariation * SignedUnit(MixHash(hash, salt)) };
			};

			return FacetCell { left, top, main_diagonal, { facet(0, FIRST_HALF), facet(1, SECOND_HALF) } };
		}

		bool SplitsAlongMainDiagonal(const FacetCell& facet, uint32_t x, uint32_t z, uint64_t cell_hash, uint32_t facet_size)
		{
			const uint32_t across = x - facet.left;
			const uint32_t down   = z - facet.top;

			if (facet.mainDiagonal && across == down)
				return true;

			if (!facet.mainDiagonal && across + down + 1 == facet_size)
				return false;

			return (cell_hash & DIAGONAL_BIT) != 0;
		}

		const Facet& FacetOf(const FacetCell& facet, const Triangle& corners, uint32_t vertices_per_side, uint32_t facet_size)
		{
			glm::vec2 centroid = {};
			for (const uint32_t corner : corners)
				centroid += glm::vec2(static_cast<float>(corner % vertices_per_side), static_cast<float>(corner / vertices_per_side));

			centroid = centroid / TRIANGLE_CORNERS - glm::vec2(static_cast<float>(facet.left), static_cast<float>(facet.top));

			const bool first_half = facet.mainDiagonal ? centroid.y > centroid.x : centroid.x + centroid.y < static_cast<float>(facet_size);
			return facet.halves[first_half ? 0 : 1];
		}

		void AppendFacet(Render::MeshData& mesh, std::span<const glm::vec4> grid_colors, uint32_t copy, const Triangle& corners, const Facet& facet)
		{
			const auto& [first, second, third] = corners;
			const uint32_t  provoking          = first + copy;
			const glm::vec4 average            = (grid_colors[first] + grid_colors[second] + grid_colors[third]) / TRIANGLE_CORNERS;

			mesh.vertices[provoking].color  = glm::vec4(glm::vec3(average) * facet.tone, average.a);
			mesh.vertices[provoking].normal = facet.normal;
			mesh.indices.insert(mesh.indices.end(), { provoking, second, third });
		}

		void AppendCell(Render::MeshData& mesh, std::span<const glm::vec4> grid_colors, const FacetCell& facet, ChunkCoord coord, uint32_t x, uint32_t z, const WorldSettings& settings)
		{
			const uint32_t vertices_per_side = settings.chunkQuads + 1;
			const uint32_t grid_size         = static_cast<uint32_t>(grid_colors.size());
			const uint64_t cell_hash         = CellHash(CELL_SEED, static_cast<int64_t>(coord.x) * settings.chunkQuads + x, static_cast<int64_t>(coord.z) * settings.chunkQuads + z);
			const bool     main_diagonal     = SplitsAlongMainDiagonal(facet, x, z, cell_hash, settings.facetSize);
			const Halves   halves            = SplitQuad(z * vertices_per_side + x, 1, vertices_per_side, main_diagonal);

			AppendFacet(mesh, grid_colors, FIRST_COPY, halves[0], FacetOf(facet, halves[0], vertices_per_side, settings.facetSize));
			AppendFacet(mesh, grid_colors, grid_size,  halves[1], FacetOf(facet, halves[1], vertices_per_side, settings.facetSize));
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
		DEBUG_ASSERT(settings.facetSize > 0 && settings.chunkQuads % settings.facetSize == 0, "Facets must evenly divide a chunk");

		const uint32_t grid_size        = static_cast<uint32_t>(mesh.vertices.size());
		const uint32_t facets_per_chunk = settings.chunkQuads / settings.facetSize;

		std::vector<glm::vec4> grid_colors(grid_size);
		std::ranges::transform(mesh.vertices, grid_colors.begin(), &Render::Vertex::color);

		mesh.vertices.reserve(grid_size * GRID_COPIES);
		for (uint32_t index = 0; index < grid_size; index++)
			mesh.vertices.push_back(mesh.vertices[index]);

		mesh.indices.reserve(mesh.indices.size() + static_cast<size_t>(settings.chunkQuads) * settings.chunkQuads * INDICES_PER_QUAD);

		for (uint32_t facet_z = 0; facet_z < facets_per_chunk; facet_z++)
		{
			for (uint32_t facet_x = 0; facet_x < facets_per_chunk; facet_x++)
			{
				const FacetCell facet = MakeFacetCell(mesh, coord, facet_x, facet_z, settings);

				for (uint32_t z = facet.top; z < facet.top + settings.facetSize; z++)
					for (uint32_t x = facet.left; x < facet.left + settings.facetSize; x++)
						AppendCell(mesh, grid_colors, facet, coord, x, z, settings);
			}
		}
	}
}
