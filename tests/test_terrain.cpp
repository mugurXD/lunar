#include <lunar/world/terrain.hpp>
#include <lunar/world/world_settings.hpp>
#include <gtest/gtest.h>

#include "world_test_types.hpp"

#include <glm/glm.hpp>

namespace
{
	constexpr float    FLAT_HEIGHT          = 5.f;
	constexpr size_t   INDICES_PER_TRIANGLE = 3;
	constexpr size_t   INDICES_PER_QUAD     = 6;
	constexpr uint32_t BORDER_SAMPLES       = 2;
	constexpr float    NORMAL_TOLERANCE     = 1e-5f;

	const glm::vec3 FLAT_COLOR = { 0.5f, 0.5f, 0.5f };
	const glm::vec3 UP         = { 0.f, 1.f, 0.f };

	const lunar::World::WorldSettings           SETTINGS = {};
	const lunar::World::RegionContext<TestPlan> CONTEXT  = lunar::World::RegionContext<TestPlan>(SETTINGS, {});

	float FlatHeight(double, double)
	{
		return FLAT_HEIGHT;
	}

	glm::vec3 FlatColor(double, double, float, const glm::vec3&)
	{
		return FLAT_COLOR;
	}

	lunar::World::ChunkData FlatChunk()
	{
		lunar::World::Heightmap heightmap = lunar::World::SampleHeightmap(FlatHeight, {}, SETTINGS);
		lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, FlatColor, {}, SETTINGS);
		return { std::move(heightmap), std::move(mesh) };
	}

	lunar::World::ChunkData WaveChunk(const WaveGenerator& generator, lunar::World::ChunkCoord coord)
	{
		lunar::World::Heightmap heightmap = lunar::World::SampleHeightmap([&](double x, double z) { return generator.sampleHeight(CONTEXT, x, z); }, coord, SETTINGS);
		lunar::Render::MeshData mesh      = lunar::World::BuildChunkMesh(heightmap, FlatColor, coord, SETTINGS);
		return { std::move(heightmap), std::move(mesh) };
	}

	uint32_t VerticesPerSide()
	{
		return SETTINGS.chunkQuads + 1;
	}

	const lunar::Render::Vertex& VertexAt(const lunar::World::ChunkData& chunk, uint32_t x, uint32_t z)
	{
		return chunk.mesh.vertices[z * VerticesPerSide() + x];
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

	EXPECT_EQ(chunk.mesh.vertices.size(),     static_cast<size_t>(VerticesPerSide()) * VerticesPerSide());
	EXPECT_EQ(chunk.mesh.indices.size(),      static_cast<size_t>(SETTINGS.chunkQuads) * SETTINGS.chunkQuads * INDICES_PER_QUAD);
	EXPECT_EQ(chunk.heightmap.samplesPerSide, VerticesPerSide() + BORDER_SAMPLES);
	EXPECT_EQ(chunk.heightmap.samplesPerSide, lunar::World::HeightmapSamplesPerSide(SETTINGS));

	for (const uint32_t index : chunk.mesh.indices)
		EXPECT_LT(index, chunk.mesh.vertices.size());
}

TEST(Terrain, FlatTerrainFacesUpAndSpansTheChunk)
{
	const lunar::World::ChunkData chunk = FlatChunk();

	for (size_t first = 0; first < chunk.mesh.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const glm::vec3& a = chunk.mesh.vertices[chunk.mesh.indices[first]].position;
		const glm::vec3& b = chunk.mesh.vertices[chunk.mesh.indices[first + 1]].position;
		const glm::vec3& c = chunk.mesh.vertices[chunk.mesh.indices[first + 2]].position;

		EXPECT_GT(glm::cross(b - a, c - a).y, 0.f);
	}

	for (const lunar::Render::Vertex& vertex : chunk.mesh.vertices)
	{
		EXPECT_EQ(vertex.position.y, FLAT_HEIGHT);
		EXPECT_NEAR(glm::length(vertex.normal - UP), 0.f, NORMAL_TOLERANCE);
		EXPECT_EQ(glm::vec3(vertex.color), FLAT_COLOR);
	}

	const lunar::Render::Vertex& far_corner = VertexAt(chunk, SETTINGS.chunkQuads, SETTINGS.chunkQuads);
	EXPECT_EQ(far_corner.position.x, SETTINGS.getChunkSize());
	EXPECT_EQ(far_corner.position.z, SETTINGS.getChunkSize());
}

TEST(Terrain, NeighbouringChunksShareEdgeHeights)
{
	const WaveGenerator generator;

	for (const lunar::World::ChunkCoord coord : { lunar::World::ChunkCoord { 0, 0 }, lunar::World::ChunkCoord { -1, -1 }, lunar::World::ChunkCoord { 5, -3 } })
	{
		const lunar::World::ChunkData chunk = WaveChunk(generator, coord);
		const lunar::World::ChunkData east  = WaveChunk(generator, { coord.x + 1, coord.z });
		const lunar::World::ChunkData south = WaveChunk(generator, { coord.x, coord.z + 1 });

		for (uint32_t edge = 0; edge < VerticesPerSide(); edge++)
		{
			EXPECT_EQ(VertexAt(chunk, SETTINGS.chunkQuads, edge).position.y, VertexAt(east,  0, edge).position.y);
			EXPECT_EQ(VertexAt(chunk, edge, SETTINGS.chunkQuads).position.y, VertexAt(south, edge, 0).position.y);
		}
	}
}
