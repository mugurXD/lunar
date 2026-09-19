#pragma once
#include <lunar/core/gameobject.hpp>

#include <glm/glm.hpp>

namespace trok
{
	struct ChaseCameraSettings
	{
		float distance   = 14.f;
		float height     = 5.f;
		float lookHeight = 2.f;
		float sharpness  = 6.f;
	};

	glm::vec3 ChaseCameraPosition(const lunar::Transform& target, const ChaseCameraSettings& settings);
	void      FollowTarget(lunar::Transform& camera, const lunar::Transform& target, const ChaseCameraSettings& settings, float delta_time);
}
