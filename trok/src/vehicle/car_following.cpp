#include <trok/vehicle/car_following.hpp>

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr float IDM_EXPONENT = 4.f;
		constexpr float MIN_GAP      = 0.1f;
	}

	float DesiredGap(float speed, float closing_speed, const DriverSettings& settings)
	{
		return settings.minimumGap + std::max(0.f, speed * settings.timeHeadway + speed * closing_speed / (2.f * std::sqrt(settings.acceleration * settings.comfortableBraking)));
	}

	float FollowingAcceleration(float speed, float desired_speed, float gap, float leader_speed, const DriverSettings& settings)
	{
		const float interaction = DesiredGap(speed, speed - leader_speed, settings) / std::max(gap, MIN_GAP);
		return settings.acceleration * (1.f - std::pow(speed / desired_speed, IDM_EXPONENT) - interaction * interaction);
	}
}
