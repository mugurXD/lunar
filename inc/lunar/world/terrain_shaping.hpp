#pragma once
#include <lunar/api.hpp>

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace lunar::World
{
	struct LUNAR_API ShapePoint
	{
		glm::dvec2 position = {};
		double     height   = 0.0;
		double     core     = 0.0;
		double     blend    = 0.0;
	};

	struct LUNAR_API ShapeDeclaration
	{
		std::vector<ShapePoint> points = {};
		double                  spread = 0.0;
		double                  reach  = 0.0;
	};

	LUNAR_API float ApplyShapes(std::span<const ShapeDeclaration> declarations, double x, double z, float height);
}
