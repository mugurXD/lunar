#pragma once
#include <glm/glm.hpp>

#include <algorithm>

namespace lunar
{
	template<typename Vector>
	auto DistanceToSegment(const Vector& point, const Vector& from, const Vector& to, typename Vector::value_type& amount)
	{
		using Scalar = typename Vector::value_type;

		const Vector direction = to - from;
		const Scalar length    = glm::dot(direction, direction);

		amount = length > Scalar(0) ? std::clamp(glm::dot(point - from, direction) / length, Scalar(0), Scalar(1)) : Scalar(0);
		return glm::distance(point, from + direction * amount);
	}
}
