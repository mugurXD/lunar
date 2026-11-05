#include <lunar/render/shadow_cascades.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace lunar::Render
{
	namespace
	{
		constexpr float HALF           = 0.5f;
		constexpr float PARALLEL_TO_UP = 0.999f;
		constexpr float PADDING_TEXELS = 1.f;

		const glm::vec3 WORLD_UP      = { 0.f, 1.f, 0.f };
		const glm::vec3 WORLD_FORWARD = { 0.f, 0.f, -1.f };
		const glm::vec4 WORLD_ORIGIN  = { 0.f, 0.f, 0.f, 1.f };

		float SplitDepth(float near_depth, float far_depth, float fraction, float blend)
		{
			const float uniform     = glm::mix(near_depth, far_depth, fraction);
			const float logarithmic = near_depth * std::pow(far_depth / near_depth, fraction);
			return glm::mix(uniform, logarithmic, blend);
		}
	}

	std::array<ShadowCascade, SHADOW_CASCADE_COUNT> FitShadowCascades(const Camera&          camera,
	                                                                  float                  aspect_ratio,
	                                                                  const glm::vec3&       light_direction,
	                                                                  const CascadedShadows& shadows,
	                                                                  uint32_t               resolution)
	{
		const glm::mat4 camera_to_world = glm::inverse(camera.getViewMatrix());
		const glm::vec3 eye             = camera_to_world[3];
		const glm::vec3 front           = -glm::vec3(camera_to_world[2]);
		const float     corner_slope    = glm::tan(glm::radians(camera.fov) * HALF) * std::sqrt(1.f + aspect_ratio * aspect_ratio);
		const glm::vec3 direction       = glm::normalize(light_direction);
		const glm::vec3 up              = std::abs(direction.y) > PARALLEL_TO_UP ? WORLD_FORWARD : WORLD_UP;
		const float     half_resolution = static_cast<float>(resolution) * HALF;

		std::array<ShadowCascade, SHADOW_CASCADE_COUNT> cascades    = {};
		const float                                     slice_count = static_cast<float>(cascades.size());
		for (size_t index = 0; index < cascades.size(); index++)
		{
			const float slice_near = SplitDepth(camera.nearPlane, shadows.distance, static_cast<float>(index) / slice_count, shadows.splitBlend);
			const float slice_far  = SplitDepth(camera.nearPlane, shadows.distance, static_cast<float>(index + 1) / slice_count, shadows.splitBlend);

			// The tightest sphere around a slice only depends on its depths and the field of view, so its size stays
			// constant while the camera turns, which together with texel snapping keeps shadow edges from shimmering.
			const float     center_depth = std::min((slice_near + slice_far) * HALF * (1.f + corner_slope * corner_slope), slice_far);
			const float     radius       = glm::length(glm::vec2(slice_far - center_depth, slice_far * corner_slope));
			const float     half_extent  = radius * half_resolution / (half_resolution - PADDING_TEXELS);
			const float     reach        = half_extent + shadows.casterMargin;
			const glm::vec3 center       = eye + front * center_depth;

			// Depth is reversed so shadow passes can share the main pass's reverse-Z depth state.
			const glm::mat4 view       = glm::lookAtRH(center - direction * reach, center, up);
			glm::mat4       projection = glm::orthoRH_ZO(-half_extent, half_extent, -half_extent, half_extent, reach + half_extent, 0.f);

			const glm::vec2 origin_texel = glm::vec2(projection * view * WORLD_ORIGIN) * half_resolution;
			const glm::vec2 snap         = (glm::round(origin_texel) - origin_texel) / half_resolution;
			projection[3][0] += snap.x;
			projection[3][1] += snap.y;

			cascades[index] = { projection * view, half_extent / half_resolution, slice_far };
		}

		return cascades;
	}
}
