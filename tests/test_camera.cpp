#include <lunar/render/components.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>

namespace
{
	constexpr int   RENDER_WIDTH    = 1280;
	constexpr int   RENDER_HEIGHT   = 720;
	constexpr float NEAR_PLANE      = 0.1f;
	constexpr float VIEW_DISTANCE   = 5.f;
	constexpr float FAR_DISTANCE    = 1000000.f;
	constexpr float TOLERANCE       = 1e-5f;
	constexpr float HALF            = 0.5f;

	glm::vec3 Project(const lunar::Camera& camera, const glm::vec3& view_position)
	{
		const glm::vec4 clip = camera.getProjectionMatrix(RENDER_WIDTH, RENDER_HEIGHT) * glm::vec4(view_position, 1.f);
		return glm::vec3(clip) / clip.w;
	}
}

TEST(CameraProjection, NearPlaneMapsToDepthOne)
{
	lunar::Camera camera;
	camera.nearPlane = NEAR_PLANE;

	EXPECT_NEAR(Project(camera, { 0.f, 0.f, -NEAR_PLANE }).z, 1.f, TOLERANCE);
}

TEST(CameraProjection, DepthDecreasesTowardsZeroWithDistance)
{
	const lunar::Camera camera;

	const float near_depth = Project(camera, { 0.f, 0.f, -VIEW_DISTANCE }).z;
	const float far_depth  = Project(camera, { 0.f, 0.f, -FAR_DISTANCE }).z;

	EXPECT_GT(near_depth, far_depth);
	EXPECT_GT(far_depth, 0.f);
	EXPECT_NEAR(far_depth, 0.f, TOLERANCE);
}

TEST(CameraProjection, ViewAxisMapsToScreenCenter)
{
	const glm::vec3 ndc = Project(lunar::Camera(), { 0.f, 0.f, -VIEW_DISTANCE });

	EXPECT_NEAR(ndc.x, 0.f, TOLERANCE);
	EXPECT_NEAR(ndc.y, 0.f, TOLERANCE);
}

TEST(CameraProjection, FieldOfViewEdgesMapToScreenEdges)
{
	const lunar::Camera camera;
	const float         aspect_ratio = static_cast<float>(RENDER_WIDTH) / static_cast<float>(RENDER_HEIGHT);
	const float         top          = VIEW_DISTANCE * glm::tan(glm::radians(camera.fov) * HALF);

	EXPECT_NEAR(Project(camera, { 0.f,                 top, -VIEW_DISTANCE }).y, 1.f, TOLERANCE);
	EXPECT_NEAR(Project(camera, { top * aspect_ratio, 0.f,  -VIEW_DISTANCE }).x, 1.f, TOLERANCE);
}
