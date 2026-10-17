#pragma once

namespace trok
{
	struct DriverSettings
	{
		float minimumGap          = 2.f;
		float timeHeadway         = 1.5f;
		float acceleration        = 1.5f;
		float comfortableBraking  = 2.f;
		float lateralAcceleration = 2.f;
		float curvatureSpan       = 20.f;
		float offRoadSpeed        = 8.f;
		float manoeuvreSpeed      = 4.f;
		float pedalAcceleration   = 6.f;
		float pedalBraking        = 8.f;
		float lookaheadTime       = 0.8f;
		float minimumLookahead    = 6.f;
		float scanDistance        = 120.f;
	};

	float DesiredGap(float speed, float closing_speed, const DriverSettings& settings);
	float FollowingAcceleration(float speed, float desired_speed, float gap, float leader_speed, const DriverSettings& settings);
}
