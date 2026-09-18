#include <lunar/render/frustum.hpp>

#include <glm/gtc/matrix_access.hpp>

#include <algorithm>

namespace lunar::Render
{
	namespace
	{
		constexpr int ROW_X = 0;
		constexpr int ROW_Y = 1;
		constexpr int ROW_Z = 2;
		constexpr int ROW_W = 3;
	}

	Bounds Bounds::transformed(const glm::mat4& transform) const
	{
		const glm::vec3 center       = (min + max) * 0.5f;
		const glm::vec3 half_extent  = (max - min) * 0.5f;
		const glm::mat3 linear       = glm::mat3(transform);
		const glm::mat3 absolute     = glm::mat3(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
		const glm::vec3 world_center = glm::vec3(transform * glm::vec4(center, 1.f));
		const glm::vec3 world_extent = absolute * half_extent;

		return Bounds { world_center - world_extent, world_center + world_extent };
	}

	Bounds Bounds::FromVertices(std::span<const Vertex> vertices)
	{
		if (vertices.empty())
			return Bounds {};

		Bounds bounds = { vertices.front().position, vertices.front().position };
		for (const Vertex& vertex : vertices)
		{
			bounds.min = glm::min(bounds.min, vertex.position);
			bounds.max = glm::max(bounds.max, vertex.position);
		}

		return bounds;
	}

	Frustum::Frustum(const glm::mat4& view_projection) noexcept
	{
		const glm::vec4 row_x = glm::row(view_projection, ROW_X);
		const glm::vec4 row_y = glm::row(view_projection, ROW_Y);
		const glm::vec4 row_z = glm::row(view_projection, ROW_Z);
		const glm::vec4 row_w = glm::row(view_projection, ROW_W);

		planes = { row_w + row_x, row_w - row_x, row_w + row_y, row_w - row_y, row_z, row_w - row_z };
	}

	bool Frustum::intersects(const Bounds& bounds) const
	{
		return std::ranges::none_of(planes, [&bounds](const glm::vec4& plane) {
			const glm::vec3 normal         = glm::vec3(plane);
			const glm::vec3 leading_corner = glm::mix(bounds.min, bounds.max, glm::greaterThanEqual(normal, glm::vec3(0.f)));
			return glm::dot(normal, leading_corner) + plane.w < 0.f;
		});
	}
}
