#pragma once
#include <lunar/api.hpp>
#include <lunar/render/common.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace lunar::Render
{
	struct LUNAR_API Bounds
	{
		glm::vec3 min = {};
		glm::vec3 max = {};

		Bounds transformed(const glm::mat4& transform) const;

		static Bounds FromVertices(std::span<const Vertex> vertices);
	};

	constexpr size_t FRUSTUM_PLANE_COUNT = 6;

	using FrustumPlanes = std::array<glm::vec4, FRUSTUM_PLANE_COUNT>;

	class LUNAR_API Frustum
	{
	public:
		Frustum(const glm::mat4& view_projection) noexcept;

		bool                 intersects(const Bounds& bounds) const;
		const FrustumPlanes& getPlanes()                      const;

	private:
		FrustumPlanes planes = {};
	};
}
