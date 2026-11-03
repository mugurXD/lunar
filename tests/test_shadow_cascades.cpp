#include <lunar/render/shadow_cascades.hpp>
#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cmath>

using lunar::Render::Bounds;
using lunar::Render::FitShadowCascades;
using lunar::Render::Frustum;
using lunar::Render::ShadowCascade;
using lunar::Render::SHADOW_CASCADE_COUNT;

namespace
{
	constexpr float    HALF            = 0.5f;
	constexpr float    ASPECT_RATIO    = 16.f / 9.f;
	constexpr uint32_t RESOLUTION      = 2048;
	constexpr float    HALF_RESOLUTION = RESOLUTION * HALF;
	constexpr float    CORNER_SIDES[]  = { -1.f, 1.f };
	constexpr float    SUB_TEXEL_STEP  = 0.37f;
	constexpr float    TEXEL_TOLERANCE = 0.01f;
	constexpr float    INSIDE_FRACTION = 0.99f;
	constexpr float    FULL_TURN       = 360.f;
	constexpr float    YAW_STEP        = 15.f;
	constexpr float    PITCHES[]       = { -75.f, -45.f, -15.f, 0.f, 15.f, 45.f, 75.f };

	using Cascades = std::array<ShadowCascade, SHADOW_CASCADE_COUNT>;

	const glm::vec3 WORLD_UP        = { 0.f, 1.f, 0.f };
	const glm::vec3 CAMERA_FORWARD  = { 0.f, 0.f, -1.f };
	const glm::vec3 SUN_DIRECTION   = { -0.4f, -1.f, -0.3f };
	const glm::vec3 SUN_OVERHEAD    = { 0.f, -1.f, 0.f };
	const glm::vec3 CAMERA_POSITION = { 1200.f, 35.f, -600.f };
	const glm::vec3 CAMERA_TARGET   = { 1500.f, 0.f, -400.f };
	const glm::vec3 TURNED_TARGET   = { 900.f, 20.f, -800.f };
	const glm::vec3 MOVE_DIRECTION  = glm::normalize(glm::vec3(1.f, 0.2f, -0.6f));

	lunar::Camera CameraLookingAt(const glm::vec3& position, const glm::vec3& target)
	{
		lunar::Camera camera;
		camera.view = glm::lookAt(position, target, WORLD_UP);
		return camera;
	}

	Cascades Fit(const lunar::Camera& camera, const glm::vec3& light_direction = SUN_DIRECTION)
	{
		return FitShadowCascades(camera, ASPECT_RATIO, light_direction, lunar::CascadedShadows {}, RESOLUTION);
	}

	bool Covers(const ShadowCascade& cascade, const glm::vec3& point)
	{
		return Frustum(cascade.viewProjection).intersects(Bounds { point, point });
	}

	glm::vec2 TexelOf(const ShadowCascade& cascade, const glm::vec3& point)
	{
		return glm::vec2(cascade.viewProjection * glm::vec4(point, 1.f)) * HALF_RESOLUTION;
	}

	void ExpectEachSliceOfTheViewCovered(const lunar::Camera& camera, const Cascades& cascades)
	{
		const glm::mat4 camera_to_world = glm::inverse(camera.view);
		const float     tan_half_y      = glm::tan(glm::radians(camera.fov) * HALF);
		const float     tan_half_x      = tan_half_y * ASPECT_RATIO;
		float           slice_near      = camera.nearPlane;

		for (size_t index = 0; index < cascades.size(); index++)
		{
			const Frustum frustum(cascades[index].viewProjection);
			for (const float depth : { slice_near, cascades[index].farDistance })
				for (const float side_x : CORNER_SIDES)
					for (const float side_y : CORNER_SIDES)
					{
						const glm::vec3 corner = camera_to_world * glm::vec4(side_x * tan_half_x * depth, side_y * tan_half_y * depth, -depth, 1.f);
						EXPECT_TRUE(frustum.intersects(Bounds { corner, corner })) << "cascade " << index << " misses its slice corner at depth " << depth;
					}

			slice_near = cascades[index].farDistance;
		}

		EXPECT_FLOAT_EQ(cascades.back().farDistance, lunar::CascadedShadows {}.distance);
	}
}

TEST(ShadowCascades, CoverTheirSliceOfTheViewInEveryDirection)
{
	for (const glm::vec3& sun_direction : { SUN_DIRECTION, SUN_OVERHEAD })
		for (float yaw = 0.f; yaw < FULL_TURN; yaw += YAW_STEP)
			for (const float pitch : PITCHES)
			{
				SCOPED_TRACE(testing::Message() << "yaw " << yaw << ", pitch " << pitch << ", sun y " << sun_direction.y);

				const glm::vec3     front  = lunar::CameraRotation(yaw, pitch) * CAMERA_FORWARD;
				const lunar::Camera camera = CameraLookingAt(CAMERA_POSITION, CAMERA_POSITION + front);
				ExpectEachSliceOfTheViewCovered(camera, Fit(camera, sun_direction));
			}
}

TEST(ShadowCascades, GetCoarserFurtherFromTheCamera)
{
	const Cascades cascades = Fit(CameraLookingAt(CAMERA_POSITION, CAMERA_TARGET));

	for (size_t index = 1; index < cascades.size(); index++)
		EXPECT_GT(cascades[index].texelSize, cascades[index - 1].texelSize);
}

TEST(ShadowCascades, IncludeCastersBetweenTheSunAndTheCamera)
{
	const Cascades  cascades = Fit(CameraLookingAt(CAMERA_POSITION, CAMERA_TARGET));
	const glm::vec3 caster   = CAMERA_POSITION - glm::normalize(SUN_DIRECTION) * lunar::CascadedShadows {}.casterMargin * INSIDE_FRACTION;

	EXPECT_TRUE(Covers(cascades.front(), caster));
}

TEST(ShadowCascades, KeepWorldPointsOnTheSameTexelsWhileTheCameraMovesAndTurns)
{
	const Cascades before = Fit(CameraLookingAt(CAMERA_POSITION, CAMERA_TARGET));

	for (size_t index = 0; index < before.size(); index++)
	{
		const glm::vec3 moved = CAMERA_POSITION + MOVE_DIRECTION * before[index].texelSize * SUB_TEXEL_STEP;
		const Cascades  after = Fit(CameraLookingAt(moved, TURNED_TARGET));
		const glm::vec2 shift = TexelOf(after[index], CAMERA_POSITION) - TexelOf(before[index], CAMERA_POSITION);

		EXPECT_FLOAT_EQ(after[index].texelSize, before[index].texelSize) << "cascade " << index;
		EXPECT_NEAR(shift.x, std::round(shift.x), TEXEL_TOLERANCE) << "cascade " << index;
		EXPECT_NEAR(shift.y, std::round(shift.y), TEXEL_TOLERANCE) << "cascade " << index;
	}
}

TEST(ShadowCascades, GivePointsCloserToTheSunAGreaterDepth)
{
	const Cascades  cascades = Fit(CameraLookingAt(CAMERA_POSITION, CAMERA_TARGET));
	const glm::vec3 raised   = CAMERA_POSITION - SUN_DIRECTION;

	for (const ShadowCascade& cascade : cascades)
		EXPECT_GT((cascade.viewProjection * glm::vec4(raised, 1.f)).z, (cascade.viewProjection * glm::vec4(CAMERA_POSITION, 1.f)).z);
}
