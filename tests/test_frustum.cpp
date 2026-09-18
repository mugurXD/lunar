#include <lunar/render/components.hpp>
#include <lunar/render/frustum.hpp>
#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <vector>

using lunar::Render::Bounds;
using lunar::Render::Frustum;
using lunar::Render::Vertex;

namespace
{
	constexpr int   VIEWPORT_WIDTH  = 16;
	constexpr int   VIEWPORT_HEIGHT = 9;
	constexpr float BOX_DISTANCE    = 50.f;
	constexpr float FAR_DISTANCE    = 1000000.f;
	constexpr float SIDE_OFFSET     = 500.f;
	constexpr float QUARTER_TURN    = 90.f;
	constexpr float TOLERANCE       = 1e-5f;

	const glm::vec3 HALF_BOX    = { 1.f, 1.f, 1.f };
	const glm::vec3 WORLD_UP    = { 0.f, 1.f, 0.f };
	const glm::vec3 TRANSLATION = { 10.f, -2.f, 3.f };

	Frustum CameraFrustum()
	{
		const lunar::Camera camera;
		return Frustum(camera.getProjectionMatrix(VIEWPORT_WIDTH, VIEWPORT_HEIGHT) * camera.getViewMatrix());
	}

	Bounds BoxAt(const glm::vec3& center, const glm::vec3& half_size = HALF_BOX)
	{
		return Bounds { center - half_size, center + half_size };
	}

	Vertex VertexAt(const glm::vec3& position)
	{
		return Vertex { .position = position };
	}
}

TEST(Frustum, BoxesInFrontOfTheCameraAreVisible)
{
	EXPECT_TRUE(CameraFrustum().intersects(BoxAt({ 0.f, 0.f, -BOX_DISTANCE })));
	EXPECT_TRUE(CameraFrustum().intersects(BoxAt({ 0.f, 0.f, -FAR_DISTANCE })));
}

TEST(Frustum, BoxesBehindOrBesideTheCameraAreCulled)
{
	EXPECT_FALSE(CameraFrustum().intersects(BoxAt({ 0.f,          0.f,          BOX_DISTANCE })));
	EXPECT_FALSE(CameraFrustum().intersects(BoxAt({ -SIDE_OFFSET, 0.f,          -BOX_DISTANCE })));
	EXPECT_FALSE(CameraFrustum().intersects(BoxAt({ SIDE_OFFSET,  0.f,          -BOX_DISTANCE })));
	EXPECT_FALSE(CameraFrustum().intersects(BoxAt({ 0.f,          SIDE_OFFSET,  -BOX_DISTANCE })));
	EXPECT_FALSE(CameraFrustum().intersects(BoxAt({ 0.f,          -SIDE_OFFSET, -BOX_DISTANCE })));
}

TEST(Frustum, BoxesCrossingAnEdgeOrContainingTheCameraAreVisible)
{
	const glm::vec3 wide = { SIDE_OFFSET, 1.f, 1.f };

	EXPECT_TRUE(CameraFrustum().intersects(BoxAt({ -SIDE_OFFSET, 0.f, -BOX_DISTANCE }, wide)));
	EXPECT_TRUE(CameraFrustum().intersects(BoxAt({ 0.f, 0.f, 0.f }, glm::vec3(BOX_DISTANCE))));
}

TEST(Bounds, VerticesAreEnclosed)
{
	const std::vector<Vertex> vertices = { VertexAt({ 1.f, -2.f, 3.f }), VertexAt({ -4.f, 5.f, 0.f }), VertexAt({ 2.f, 0.f, -6.f }) };
	const Bounds              bounds   = Bounds::FromVertices(vertices);

	EXPECT_EQ(bounds.min, glm::vec3(-4.f, -2.f, -6.f));
	EXPECT_EQ(bounds.max, glm::vec3(2.f, 5.f, 3.f));
}

TEST(Bounds, TransformsMoveAndRotateTheBox)
{
	const Bounds    box          = { { 0.f, 0.f, 0.f }, { 4.f, 1.f, 2.f } };
	const Bounds    moved        = box.transformed(glm::translate(glm::mat4(1.f), TRANSLATION));
	const Bounds    rotated      = box.transformed(glm::rotate(glm::mat4(1.f), glm::radians(QUARTER_TURN), WORLD_UP));
	const glm::vec3 rotated_size = rotated.max - rotated.min;

	EXPECT_EQ(moved.min, TRANSLATION);
	EXPECT_EQ(moved.max, box.max + TRANSLATION);
	EXPECT_NEAR(rotated_size.x, 2.f, TOLERANCE);
	EXPECT_NEAR(rotated_size.y, 1.f, TOLERANCE);
	EXPECT_NEAR(rotated_size.z, 4.f, TOLERANCE);
}
