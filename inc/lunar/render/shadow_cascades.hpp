#pragma once
#include <lunar/api.hpp>
#include <lunar/render/components.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace lunar::Render
{
	constexpr size_t SHADOW_CASCADE_COUNT = 3;

	struct LUNAR_API ShadowCascade
	{
		glm::mat4 viewProjection = glm::mat4(1.f);
		float     texelSize      = 0.f;
		float     farDistance    = 0.f;
	};

	LUNAR_API std::array<ShadowCascade, SHADOW_CASCADE_COUNT> FitShadowCascades(const Camera&          camera,
	                                                                            float                  aspect_ratio,
	                                                                            const glm::vec3&       light_direction,
	                                                                            const CascadedShadows& shadows,
	                                                                            uint32_t               resolution);
}
