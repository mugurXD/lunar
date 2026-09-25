#include <lunar/world/terrain_shaping.hpp>
#include <lunar/utils/geometry.hpp>

#include <algorithm>
#include <limits>

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
			double nearest = std::numeric_limits<double>::max();
			double amount  = 0.0;
			size_t segment = 0;

			for (size_t index = 0; index + 1 < declaration.points.size(); index++)
			{
				const ShapePoint& from = declaration.points[index];
				const ShapePoint& to   = declaration.points[index + 1];
				if (OutOfReach(point, from, to, declaration.reach))
					continue;

				double       along    = 0.0;
				const double distance = DistanceToSegment(point, from.position, to.position, along);
				if (distance < nearest)
				{
					nearest = distance;
					amount  = along;
					segment = index;
				}
			}

			if (nearest > declaration.reach)
				return height;

			const ShapePoint& from   = declaration.points[segment];
			const ShapePoint& to     = declaration.points[segment + 1];
			const double      target = glm::mix(from.height, to.height, amount);
			if (height <= target)
				return height;

			const double core  = glm::mix(from.core, to.core, amount);
			const double blend = std::min(glm::mix(from.blend, to.blend, amount) + (height - target) * declaration.spread, declaration.reach - core);
			if (nearest > core + std::max(blend, 0.0))
				return height;

			const double across = nearest <= core || blend <= 0.0 ? 0.0 : (nearest - core) / blend;
			return static_cast<float>(glm::mix(target, static_cast<double>(height), glm::smoothstep(0.0, 1.0, across)));
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
