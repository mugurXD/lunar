#include <lunar/world/terrain.hpp>
#include <lunar/utils/hash.hpp>
#include <lunar/debug.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <span>

namespace lunar::World
{
	namespace
	{
		constexpr uint32_t BORDER_SIDES        = 2;
		constexpr float    CENTRAL_DIFFERENCES = 2.f;
		constexpr float    TRIANGLE_CORNERS    = 3.f;
		constexpr float    HALF                = 0.5f;
		constexpr uint64_t DIAGONAL_BIT        = 1;
		constexpr uint64_t CHUNK_SEED          = 0;
		constexpr uint64_t TRIANGLE_SEED       = 1;
		constexpr int      UNIT_BITS           = 24;
		constexpr int      UNIT_SHIFT          = 64 - UNIT_BITS;
		constexpr float    UNIT_SCALE          = 1.f / static_cast<float>(1u << UNIT_BITS);
		constexpr float    SIGNED_RANGE        = 2.f;
		constexpr int32_t  CENTROID_SCALE      = 3;

		using GridPoint    = glm::ivec2;
		using GridTriangle = std::array<GridPoint, 3>;
		using GridEdge     = std::array<GridPoint, 2>;

		struct Simplifier
		{
			std::span<const Render::Vertex> grid   = {};
			int32_t                         quads  = 0;
			std::vector<float>              errors = {};
		};

		uint64_t PointHash(uint64_t seed, int64_t x, int64_t z)
		{
			return MixHash(MixHash(seed, static_cast<uint64_t>(x)), static_cast<uint64_t>(z));
		}

		float SignedUnit(uint64_t hash)
		{
			return static_cast<float>(hash >> UNIT_SHIFT) * UNIT_SCALE * SIGNED_RANGE - 1.f;
		}

		size_t IndexOf(const Simplifier& simplifier, GridPoint point)
		{
			return static_cast<size_t>(point.y) * (simplifier.quads + 1) + point.x;
		}

		float HeightAt(const Simplifier& simplifier, GridPoint point)
		{
			return simplifier.grid[IndexOf(simplifier, point)].position.y;
		}

		std::optional<GridEdge> BorderLineOf(const Simplifier& simplifier, GridPoint from, GridPoint to)
		{
			for (const int32_t line : { 0, simplifier.quads })
			{
				if (from.x == line && to.x == line)
					return GridEdge { GridPoint { line, 0 }, GridPoint { line, simplifier.quads } };

				if (from.y == line && to.y == line)
					return GridEdge { GridPoint { 0, line }, GridPoint { simplifier.quads, line } };
			}

			return std::nullopt;
		}

		float LowestAlong(const Simplifier& simplifier, const GridEdge& line)
		{
			const GridPoint step   = glm::sign(line[1] - line[0]);
			float           lowest = HeightAt(simplifier, line[0]);

			for (GridPoint point = line[0]; point != line[1]; point += step)
				lowest = std::min(lowest, HeightAt(simplifier, point + step));

			return lowest;
		}

		bool CanSplit(GridPoint apex, GridPoint corner)
		{
			return std::abs(apex.x - corner.x) + std::abs(apex.y - corner.y) > 1;
		}

		std::vector<std::vector<GridTriangle>> SplitLevels(std::span<const GridTriangle> roots)
		{
			std::vector<std::vector<GridTriangle>> levels = { std::vector<GridTriangle>(roots.begin(), roots.end()) };

			while (true)
			{
				const auto& [first, second, apex] = levels.back().front();
				if (!CanSplit(apex, (first + second) / 2))
					return levels;

				std::vector<GridTriangle> children;
				for (const auto& [parent_first, parent_second, parent_apex] : levels.back())
				{
					const GridPoint middle = (parent_first + parent_second) / 2;
					children.push_back({ parent_apex, parent_first, middle });
					children.push_back({ parent_second, parent_apex, middle });
				}

				levels.push_back(std::move(children));
			}
		}

		void MeasureErrors(Simplifier& simplifier, std::span<const GridTriangle> roots)
		{
			const std::vector<std::vector<GridTriangle>> levels = SplitLevels(roots);

			for (size_t level = levels.size(); level-- > 0;)
			{
				const bool has_children = level + 1 < levels.size();

				for (const auto& [first, second, apex] : levels[level])
				{
					const GridPoint middle = (first + second) / 2;
					const float     own    = std::abs((HeightAt(simplifier, first) + HeightAt(simplifier, second)) * HALF - HeightAt(simplifier, middle));
					float&          error  = simplifier.errors[IndexOf(simplifier, middle)];

					error = std::max(error, own);
					if (has_children)
						error = std::max({ error, simplifier.errors[IndexOf(simplifier, (apex + first) / 2)], simplifier.errors[IndexOf(simplifier, (second + apex) / 2)] });
				}
			}
		}

		void CollectTriangles(const Simplifier& simplifier, float max_error, GridPoint first, GridPoint second, GridPoint apex, std::vector<GridTriangle>& triangles)
		{
			const GridPoint middle = (first + second) / 2;
			if (CanSplit(first, apex) && simplifier.errors[IndexOf(simplifier, middle)] > max_error)
			{
				CollectTriangles(simplifier, max_error, apex, first, middle, triangles);
				CollectTriangles(simplifier, max_error, second, apex, middle, triangles);
				return;
			}

			triangles.push_back({ first, second, apex });
		}

		std::array<GridTriangle, 2> RootTriangles(int32_t quads, bool main_diagonal)
		{
			const GridPoint near_left  = { 0, 0 };
			const GridPoint near_right = { quads, 0 };
			const GridPoint far_left   = { 0, quads };
			const GridPoint far_right  = { quads, quads };

			if (main_diagonal)
				return { GridTriangle { near_left, far_right, near_right }, GridTriangle { far_right, near_left, far_left } };

			return { GridTriangle { near_right, far_left, near_left }, GridTriangle { far_left, near_right, far_right } };
		}

		void AppendVertex(Render::MeshData& mesh, Render::Vertex vertex, const glm::vec3& normal, const glm::vec4& color)
		{
			vertex.normal = normal;
			vertex.color  = color;

			mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size()));
			mesh.vertices.push_back(vertex);
		}

		void AppendSkirt(Render::MeshData& mesh, const Simplifier& simplifier, GridPoint from, GridPoint to, const glm::vec3& normal, const glm::vec4& color)
		{
			const std::optional<GridEdge> line = BorderLineOf(simplifier, from, to);
			if (!line.has_value())
				return;

			const float lowest = LowestAlong(simplifier, *line);
			if (HeightAt(simplifier, from) <= lowest && HeightAt(simplifier, to) <= lowest)
				return;

			const Render::Vertex top_from    = simplifier.grid[IndexOf(simplifier, from)];
			const Render::Vertex top_to      = simplifier.grid[IndexOf(simplifier, to)];
			Render::Vertex       bottom_from = top_from;
			Render::Vertex       bottom_to   = top_to;
			bottom_from.position.y = lowest;
			bottom_to.position.y   = lowest;

			const auto append_both_sides = [&](const Render::Vertex& first, const Render::Vertex& second, const Render::Vertex& third) {
				if (first.position == second.position || second.position == third.position || third.position == first.position)
					return;

				for (const auto& [a, b, c] : { std::array { first, second, third }, std::array { first, third, second } })
				{
					AppendVertex(mesh, a, normal, color);
					AppendVertex(mesh, b, normal, color);
					AppendVertex(mesh, c, normal, color);
				}
			};

			append_both_sides(top_from, bottom_from, top_to);
			append_both_sides(top_to, bottom_from, bottom_to);
		}

		void AppendTriangle(Render::MeshData& mesh, const Simplifier& simplifier, GridTriangle corners, ChunkCoord coord, const WorldSettings& settings)
		{
			const auto position = [&](GridPoint point) { return simplifier.grid[IndexOf(simplifier, point)].position; };

			glm::vec3 facing = glm::cross(position(corners[1]) - position(corners[0]), position(corners[2]) - position(corners[0]));
			if (facing.y < 0.f)
			{
				std::swap(corners[1], corners[2]);
				facing = -facing;
			}

			const GridPoint summed = corners[0] + corners[1] + corners[2];
			const uint64_t  hash   = PointHash(TRIANGLE_SEED, static_cast<int64_t>(coord.x) * simplifier.quads * CENTROID_SCALE + summed.x, static_cast<int64_t>(coord.z) * simplifier.quads * CENTROID_SCALE + summed.y);
			const float     tone   = 1.f + settings.facetVariation * SignedUnit(hash);

			glm::vec4 average = {};
			for (const GridPoint corner : corners)
				average += simplifier.grid[IndexOf(simplifier, corner)].color;

			average /= TRIANGLE_CORNERS;

			const glm::vec3 normal = glm::normalize(facing);
			const glm::vec4 color  = glm::vec4(glm::vec3(average) * tone, average.a);

			for (const GridPoint corner : corners)
				AppendVertex(mesh, simplifier.grid[IndexOf(simplifier, corner)], normal, color);

			for (size_t corner = 0; corner < corners.size(); corner++)
				AppendSkirt(mesh, simplifier, corners[corner], corners[(corner + 1) % corners.size()], normal, color);
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
		DEBUG_ASSERT(std::has_single_bit(settings.chunkQuads), "Simplifying terrain needs a power-of-two number of quads per chunk");

		const std::vector<Render::Vertex> grid = std::move(mesh.vertices);
		const int32_t                     quads = static_cast<int32_t>(settings.chunkQuads);

		Simplifier simplifier = { .grid = grid, .quads = quads, .errors = std::vector<float>(grid.size(), 0.f) };

		const bool                        main_diagonal = (PointHash(CHUNK_SEED, coord.x, coord.z) & DIAGONAL_BIT) != 0;
		const std::array<GridTriangle, 2> roots         = RootTriangles(quads, main_diagonal);

		MeasureErrors(simplifier, roots);

		std::vector<GridTriangle> triangles;
		for (const auto& [first, second, apex] : roots)
			CollectTriangles(simplifier, settings.maxFacetError, first, second, apex, triangles);

		mesh.vertices.clear();
		mesh.indices.clear();
		for (const GridTriangle& triangle : triangles)
			AppendTriangle(mesh, simplifier, triangle, coord, settings);
	}
}
