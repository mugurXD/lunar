#include <lunar/render/mesh_registry.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

namespace
{
	constexpr size_t CUBE_FACE_COUNT       = 6;
	constexpr size_t VERTICES_PER_FACE     = 4;
	constexpr size_t INDICES_PER_FACE      = 6;
	constexpr size_t INDICES_PER_TRIANGLE  = 3;
	constexpr float  NORMAL_TOLERANCE      = 1e-5f;
}

TEST(CubeMeshData, HasFourVerticesAndTwoTrianglesPerFace)
{
	const lunar::Render::MeshData cube = lunar::Render::CreateCubeMeshData();

	EXPECT_EQ(cube.vertices.size(), CUBE_FACE_COUNT * VERTICES_PER_FACE);
	EXPECT_EQ(cube.indices.size(),  CUBE_FACE_COUNT * INDICES_PER_FACE);
}

TEST(CubeMeshData, IndicesReferenceExistingVertices)
{
	const lunar::Render::MeshData cube = lunar::Render::CreateCubeMeshData();

	for (const uint32_t index : cube.indices)
		EXPECT_LT(index, cube.vertices.size());
}

TEST(CubeMeshData, NormalsAreUnitLength)
{
	for (const lunar::Render::Vertex& vertex : lunar::Render::CreateCubeMeshData().vertices)
		EXPECT_NEAR(glm::length(vertex.normal), 1.f, NORMAL_TOLERANCE);
}

TEST(CubeMeshData, TrianglesAreCounterClockwiseSeenFromOutside)
{
	const lunar::Render::MeshData cube = lunar::Render::CreateCubeMeshData();

	for (size_t first = 0; first < cube.indices.size(); first += INDICES_PER_TRIANGLE)
	{
		const lunar::Render::Vertex& a = cube.vertices[cube.indices[first]];
		const lunar::Render::Vertex& b = cube.vertices[cube.indices[first + 1]];
		const lunar::Render::Vertex& c = cube.vertices[cube.indices[first + 2]];

		const glm::vec3 winding_normal = glm::cross(b.position - a.position, c.position - a.position);

		EXPECT_GT(glm::dot(winding_normal, a.normal), 0.f);
		EXPECT_GT(glm::dot(a.position,     a.normal), 0.f);
	}
}
