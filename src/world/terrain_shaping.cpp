#include <lunar/world/terrain_shaping.hpp>
#include <lunar/utils/geometry.hpp>

#include <algorithm>

namespace lunar::World
{
	namespace
	{
		bool OutOfReach(const glm::dvec2& point, const ShapePoint& from, const ShapePoint& to, double reach)
		{
			return point.x < std::min(from.position.x, to.position.x) - reach || point.x > std::max(from.position.x, to.position.x) + reach ||
			       point.y < std::min(from.position.y, to.position.y) - reach || point.y > std::max(from.position.y, to.position.y) + reach;
		}

		float ApplyShape(const ShapeDeclaration& declaration, const glm::dvec2& point, float height)
		{
			double carved = height;

			for (size_t index = 0; index + 1 < declaration.points.size(); index++)
			{
				const ShapePoint& from = declaration.points[index];
				const ShapePoint& to   = declaration.points[index + 1];
				if (OutOfReach(point, from, to, declaration.reach))
					continue;

				double       amount   = 0.0;
				const double distance = DistanceToSegment(point, from.position, to.position, amount);
				const double target   = glm::mix(from.height, to.height, amount);
				if (height <= target)
					continue;

				const double core  = glm::mix(from.core, to.core, amount);
				const double blend = std::min(glm::mix(from.blend, to.blend, amount) + (height - target) * declaration.spread, declaration.reach - core);
				if (distance > core + std::max(blend, 0.0))
					continue;

				const double across = distance <= core || blend <= 0.0 ? 0.0 : (distance - core) / blend;
				carved = std::min(carved, glm::mix(target, static_cast<double>(height), glm::smoothstep(0.0, 1.0, across)));
			}

			return static_cast<float>(carved);
		}
	}

	float ApplyShapes(std::span<const ShapeDeclaration> declarations, double x, double z, float height)
	{
		const glm::dvec2 point  = { x, z };
		float            carved = height;

		for (const ShapeDeclaration& declaration : declarations)
			carved = std::min(carved, ApplyShape(declaration, point, height));

		return carved;
	}
}
