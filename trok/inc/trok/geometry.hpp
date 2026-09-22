#pragma once
#include <glm/glm.hpp>

#include <algorithm>

namespace trok
{
	template<typename Vector>
	auto DistanceToSegment(const Vector& point, const Vector& from, const Vector& to, decltype(from.x)& amount)
	{
		const Vector direction = to - from;
		const auto   length    = glm::dot(direction, direction);

		amount = length > 0 ? std::clamp(glm::dot(point - from, direction) / length, decltype(length)(0), decltype(length)(1)) : decltype(length)(0);
		return glm::distance(point, from + direction * amount);
	}
}
