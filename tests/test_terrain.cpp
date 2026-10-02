#include <lunar/world/terrain.hpp>
#include <lunar/world/world_settings.hpp>
#include <gtest/gtest.h>

#include "world_test_types.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
	constexpr float    FLAT_HEIGHT          = 5.f;
	constexpr size_t   INDICES_PER_TRIANGLE = 3;
	constexpr uint32_t BORDER_SAMPLES       = 2;
	constexpr float    NORMAL_TOLERANCE     = 1e-5f;
	constexpr float    COLOR_TOLERANCE      = 1e-5f;
	constexpr float    AREA_TOLERANCE       = 1e-2f;
	constexpr float    GRADIENT_SCALE       = 1.f / 256.f;
	constexpr float    TRIANGLE_CORNERS     = 3.f;
	constexpr float    HALF                 = 0.5f;
	constexpr float    STRICT_ERROR         = 0.01f;
	constexpr size_t   FULL_RESOLUTION      = 2;
	constexpr int32_t  LAYOUT_SAMPLES       = 8;
	constexpr float    VERTICAL_TOLERANCE   = 1e-4f;
	constexpr float    SAMPLE_TOLERANCE     = 1e-3f;
	constexpr float    BUMP_HEIGHT          = 5.f;

	const glm::vec3 FLAT_COLOR = { 0.5f, 0.5f, 0.5f };
	const glm::vec3 UP         = { 0.f, 1.f, 0.f };
	const float     MISSING    = std::numeric_limits<float>::quiet_NaN();

	const lunar::World::WorldSettings           SETTINGS = { .facetVariation = 0.f };
	const lunar::World::WorldSettings           VARIED   = {};
	const lunar::World::WorldSettings           STRICT   = { .facetVariation = 0.f, .maxFacetError = STRICT_ERROR };
	const lunar::World::RegionContext<TestPlan> CONTEXT  = lunar::World::RegionContext<TestPlan>(SETTINGS, {});

	float FlatHeight(double, double)
	{
		return FLAT_HEIGHT;
	}

	glm::vec3 FlatColor(double, double, float, const glm::vec3&)
	{
		return FLAT_COLOR;
	}

	glm::vec3 GradientColor(double x, double z, float, const glm::vec3&)
	{
		return glm::vec3(static_cast<float>(x), static_cast<float>(z), 0.f) * GRADIENT_SCALE;
	}

	lunar::World::ChunkData FlatChunk(lunar::World::ChunkCoord coord = {})
	{
		lunar::World::Heightmap heightmap = lunar::World::SampleHeightmap(FlatHeight, coord, SETTINGS);
		lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, FlatColor, coord, SETTINGS);
		return { std::move(heightmap), std::move(mesh) };
	}

	lunar::World::ChunkData WaveChunk(const WaveGenerator& generator, lunar::World::ChunkCoord coord, const lunar::World::WorldSettings& settings = SETTINGS)
	{
		lunar::World::Heightmap heightmap = lunar::World::SampleHeightmap([&](double x, double z) { return generator.sampleHeight(CONTEXT, x, z); }, coord, settings);
		lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, FlatColor, coord, settings);
		return { std::move(heightmap), std::move(mesh) };
	}

	uint32_t VerticesPerSide()
	{
		return SETTINGS.chunkQuads + 1;
	}

	size_t TriangleCount(const lunar::World::ChunkData& chunk)
	{
		return chunk.mesh.indices.size() / INDICES_PER_TRIANGLE;
	}

	std::array<glm::vec3, 3> CornersOf(const lunar::Render::MeshData& mesh, size_t first)
	{
		return { mesh.vertices[mesh.indices[first]].position, mesh.vertices[mesh.indices[first + 1]].position, mesh.vertices[mesh.indices[first + 2]].position };
	}

	glm::ivec2 GridPointOf(const glm::vec3& position)
	{
		return glm::ivec2(glm::round(glm::vec2(position.x, position.z) / SETTINGS.vertexSpacing));
	}

	bool IsSkirt(const std::array<glm::vec3, 3>& corners)
	{
		const auto& [a, b, c] = corners;
		return std::abs(glm::normalize(glm::cross(b - a, c - a)).y) < VERTICAL_TOLERANCE;
	}

	template<typename Visit>
	void ForEachSurfaceTriangle(const lunar::Render::MeshData& mesh, Visit visit)
	{
		for (size_t first = 0; first < mesh.indices.size(); first += INDICES_PER_TRIANGLE)
			if (!IsSkirt(CornersOf(mesh, first)))
				visit(first, CornersOf(mesh, first));
	}

	size_t CountJunctions(const lunar::Render::MeshData& mesh)
	{
		std::vector<glm::ivec2>                points;
		std::vector<std::array<glm::ivec2, 2>> edges;

		ForEachSurfaceTriangle(mesh, [&](size_t, const std::array<glm::vec3, 3>& corners) {
			for (size_t corner = 0; corner < corners.size(); corner++)
			{
				points.push_back(GridPointOf(corners[corner]));
				edges.push_back({ GridPointOf(corners[corner]), GridPointOf(corners[(corner + 1) % corners.size()]) });
			}
		});

		std::ranges::sort(points, {}, [](glm::ivec2 point) { return std::pair(point.x, point.y); });
		points.erase(std::unique(points.begin(), points.end()), points.end());

		size_t junctions = 0;
		for (const auto& [from, to] : edges)
		{
			const glm::ivec2 span = to - from;
			for (const glm::ivec2 point : points)
			{
				const glm::ivec2 offset    = point - from;
				const bool       collinear = span.x * offset.y - span.y * offset.x == 0;
				const int32_t    along     = span.x * offset.x + span.y * offset.y;
				if (collinear && along > 0 && along < span.x * span.x + span.y * span.y)
					junctions++;
			}
		}

		return junctions;
	}

	bool OnSameBorder(glm::ivec2 from, glm::ivec2 to)
	{
		const int32_t far_edge = static_cast<int32_t>(SETTINGS.chunkQuads);
		return (from.x == to.x && (from.x == 0 || from.x == far_edge)) || (from.y == to.y && (from.y == 0 || from.y == far_edge));
	}

	float LowestOnBorder(const lunar::World::Heightmap& heightmap, glm::ivec2 from, glm::ivec2 to)
	{
		const bool along_x = from.y == to.y;
		float      lowest  = std::numeric_limits<float>::max();

		for (int32_t sample = 0; sample <= static_cast<int32_t>(SETTINGS.chunkQuads); sample++)
			lowest = std::min(lowest, along_x ? heightmap.sample(sample, from.y) : heightmap.sample(from.x, sample));

		return lowest;
	}
}

TEST(Terrain, ChunkAtUsesFloorDivision)
{
	const float chunk_size = SETTINGS.getChunkSize();

	EXPECT_EQ(lunar::World::ChunkAt(glm::vec3(0.f,                0.f, 0.f),         SETTINGS), (lunar::World::ChunkCoord { 0,  0 }));
	EXPECT_EQ(lunar::World::ChunkAt(glm::vec3(chunk_size - 0.1f,  0.f, chunk_size),  SETTINGS), (lunar::World::ChunkCoord { 0,  1 }));
	EXPECT_EQ(lunar::World::ChunkAt(glm::vec3(-0.1f,              0.f, -chunk_size), SETTINGS), (lunar::World::ChunkCoord { -1, -1 }));
	EXPECT_EQ(lunar::World::ChunkAt(glm::vec3(-chunk_size - 0.1f, 0.f, 0.f),         SETTINGS), (lunar::World::ChunkCoord { -2, 0 }));
}

TEST(Terrain, ChunkOriginRoundTripsThroughChunkAt)
{
	for (const lunar::World::ChunkCoord coord : { lunar::World::ChunkCoord { 0, 0 }, lunar::World::ChunkCoord { 3, -7 }, lunar::World::ChunkCoord { -12, 5 } })
		EXPECT_EQ(lunar::World::ChunkAt(lunar::World::ChunkOrigin(coord, SETTINGS), SETTINGS), coord);
}

TEST(Terrain, GeneratedChunkHasExpectedSizes)
{
	const lunar::World::ChunkData chunk = FlatChunk();

	EXPECT_EQ(chunk.mesh.indices.size() % INDICES_PER_TRIANGLE, 0u);
	EXPECT_EQ(chunk.mesh.indices.size(),      chunk.mesh.vertices.size());
	EXPECT_EQ(chunk.heightmap.samplesPerSide, VerticesPerSide() + BORDER_SAMPLES);
	EXPECT_EQ(chunk.heightmap.samplesPerSide, lunar::World::HeightmapSamplesPerSide(SETTINGS));

	for (const uint32_t index : chunk.mesh.indices)
		EXPECT_LT(index, chunk.mesh.vertices.size());
}

TEST(Terrain, FlatTerrainFacesUpAndCoversTheChunk)
{
	const lunar::World::ChunkData chunk = FlatChunk();
	float                         area  = 0.f;

	for (size_t first = 0; first < chunk.mesh.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const auto [a, b, c] = CornersOf(chunk.mesh, first);
		const float facing   = glm::cross(b - a, c - a).y;

		EXPECT_GT(facing, 0.f) << "flat terrain needs no skirts";
		area += facing * HALF;
	}

	for (const lunar::Render::Vertex& vertex : chunk.mesh.vertices)
	{
		EXPECT_EQ(vertex.position.y, FLAT_HEIGHT);
		EXPECT_NEAR(glm::length(vertex.normal - UP), 0.f, NORMAL_TOLERANCE);
		EXPECT_EQ(glm::vec3(vertex.color), FLAT_COLOR);
	}

	EXPECT_NEAR(area, SETTINGS.getChunkSize() * SETTINGS.getChunkSize(), AREA_TOLERANCE) << "the triangles must cover the chunk without gaps or overlaps";
}

TEST(Terrain, FlatInteriorsCollapseIntoBigTriangles)
{
	const lunar::World::ChunkData flat         = FlatChunk();
	float                         longest_edge = 0.f;

	for (size_t first = 0; first < flat.mesh.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const auto [a, b, c] = CornersOf(flat.mesh, first);
		longest_edge = std::max({ longest_edge, glm::distance(a, b), glm::distance(b, c), glm::distance(c, a) });
	}

	EXPECT_LT(TriangleCount(flat), static_cast<size_t>(SETTINGS.chunkQuads) * SETTINGS.chunkQuads * FULL_RESOLUTION);
	EXPECT_GE(longest_edge, SETTINGS.getChunkSize());
}

TEST(Terrain, CurvedTerrainKeepsMoreDetailThanFlatTerrain)
{
	const WaveGenerator generator;

	EXPECT_GT(TriangleCount(WaveChunk(generator, {})),         TriangleCount(FlatChunk()));
	EXPECT_GT(TriangleCount(WaveChunk(generator, {}, STRICT)), TriangleCount(WaveChunk(generator, {})));
}

TEST(Terrain, SimplifiedVerticesAreHeightmapSamples)
{
	const WaveGenerator           generator;
	const lunar::World::ChunkData chunk = WaveChunk(generator, {});

	ForEachSurfaceTriangle(chunk.mesh, [&](size_t, const std::array<glm::vec3, 3>& corners) {
		for (const glm::vec3& corner : corners)
		{
			const glm::ivec2 point = GridPointOf(corner);
			EXPECT_EQ(corner.y, chunk.heightmap.sample(point.x, point.y));
		}
	});
}

TEST(Terrain, IsolatedBumpsDoNotCrackTheMesh)
{
	const int32_t quads = static_cast<int32_t>(SETTINGS.chunkQuads);
	size_t        cracked = 0;

	for (int32_t bump_z = 1; bump_z < quads; bump_z++)
	{
		for (int32_t bump_x = 1; bump_x < quads; bump_x++)
		{
			const auto bumpy = [&](double x, double z) {
				return GridPointOf(glm::vec3(static_cast<float>(x), 0.f, static_cast<float>(z))) == glm::ivec2(bump_x, bump_z) ? BUMP_HEIGHT : 0.f;
			};

			lunar::World::Heightmap       heightmap = lunar::World::SampleHeightmap(bumpy, {}, SETTINGS);
			const lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, FlatColor, {}, SETTINGS);
			if (CountJunctions(mesh) > 0)
				cracked++;
		}
	}

	EXPECT_EQ(cracked, 0u);
}

TEST(Terrain, TrianglesAreLitByTheirOwnPlane)
{
	const WaveGenerator           generator;
	const lunar::World::ChunkData chunk = WaveChunk(generator, {});

	ForEachSurfaceTriangle(chunk.mesh, [&](size_t first, const std::array<glm::vec3, 3>& corners) {
		const auto& [a, b, c] = corners;
		const glm::vec3 plane = glm::normalize(glm::cross(b - a, c - a));

		for (size_t corner = 0; corner < INDICES_PER_TRIANGLE; corner++)
			EXPECT_NEAR(glm::length(chunk.mesh.vertices[chunk.mesh.indices[first + corner]].normal - plane), 0.f, NORMAL_TOLERANCE);
	});
}

TEST(Terrain, EachTriangleIsColouredByTheAverageOfItsCorners)
{
	lunar::World::Heightmap       heightmap = lunar::World::SampleHeightmap(FlatHeight, {}, SETTINGS);
	const lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, GradientColor, {}, SETTINGS);

	for (size_t first = 0; first < mesh.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const auto [a, b, c]     = CornersOf(mesh, first);
		const glm::vec3 centroid = (a + b + c) / TRIANGLE_CORNERS;

		EXPECT_NEAR(glm::length(glm::vec3(mesh.vertices[mesh.indices[first]].color) - GradientColor(centroid.x, centroid.z, 0.f, UP)), 0.f, COLOR_TOLERANCE);
	}
}

TEST(Terrain, TriangleBrightnessVariesWithinTheConfiguredRange)
{
	const WaveGenerator           generator;
	const lunar::Render::MeshData mesh      = WaveChunk(generator, {}, VARIED).mesh;
	const float                   darkest   = FLAT_COLOR.r * (1.f - VARIED.facetVariation);
	const float                   brightest = FLAT_COLOR.r * (1.f + VARIED.facetVariation);
	float                         lowest    = brightest;
	float                         highest   = darkest;

	for (size_t first = 0; first < mesh.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const float tone = mesh.vertices[mesh.indices[first]].color.r;
		EXPECT_GE(tone, darkest   - COLOR_TOLERANCE);
		EXPECT_LE(tone, brightest + COLOR_TOLERANCE);
		lowest  = std::min(lowest,  tone);
		highest = std::max(highest, tone);
	}

	EXPECT_LT(lowest, FLAT_COLOR.r);
	EXPECT_GT(highest, FLAT_COLOR.r);
}

TEST(Terrain, TriangleLayoutVariesBetweenChunks)
{
	const lunar::World::ChunkData reference = FlatChunk();
	size_t                        different = 0;

	for (int32_t offset = 1; offset <= LAYOUT_SAMPLES; offset++)
	{
		const lunar::World::ChunkData other = FlatChunk({ offset, -offset });
		if (!std::ranges::equal(other.mesh.vertices, reference.mesh.vertices, {}, &lunar::Render::Vertex::position, &lunar::Render::Vertex::position))
			different++;
	}

	EXPECT_GT(different, 0u);
}

TEST(Terrain, ChunkEdgesAreSealedBySkirtsReachingTheLowestEdgeSample)
{
	const WaveGenerator generator;

	for (const lunar::World::ChunkCoord coord : { lunar::World::ChunkCoord { 0, 0 }, lunar::World::ChunkCoord { -1, -1 }, lunar::World::ChunkCoord { 5, -3 } })
	{
		const lunar::World::ChunkData           chunk = WaveChunk(generator, coord);
		std::vector<std::array<glm::vec3, 3>>   skirts;

		for (size_t first = 0; first < chunk.mesh.indices.size(); first += INDICES_PER_TRIANGLE)
			if (IsSkirt(CornersOf(chunk.mesh, first)))
				skirts.push_back(CornersOf(chunk.mesh, first));

		ForEachSurfaceTriangle(chunk.mesh, [&](size_t, const std::array<glm::vec3, 3>& corners) {
			for (size_t corner = 0; corner < corners.size(); corner++)
			{
				const glm::vec3& from = corners[corner];
				const glm::vec3& to   = corners[(corner + 1) % corners.size()];
				if (!OnSameBorder(GridPointOf(from), GridPointOf(to)))
					continue;

				const float lowest = LowestOnBorder(chunk.heightmap, GridPointOf(from), GridPointOf(to));
				if (from.y <= lowest && to.y <= lowest)
					continue;

				const bool sealed = std::ranges::any_of(skirts, [&](const std::array<glm::vec3, 3>& skirt) {
					const auto touches = [&](const glm::vec3& point) { return std::ranges::any_of(skirt, [&](const glm::vec3& at) { return glm::distance(at, point) < SAMPLE_TOLERANCE; }); };
					const auto bottom  = std::ranges::min(skirt, {}, &glm::vec3::y);
					return touches(from) && touches(to) && std::abs(bottom.y - lowest) < SAMPLE_TOLERANCE;
				});

				EXPECT_TRUE(sealed) << "the border edge from (" << from.x << ", " << from.z << ") to (" << to.x << ", " << to.z << ") has no skirt";
			}
		});
	}
}
