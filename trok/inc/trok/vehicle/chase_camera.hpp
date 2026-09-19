#pragma once
#include <lunar/core/gameobject.hpp>

#include <glm/glm.hpp>

namespace trok
{
	struct ChaseCameraSettings
	{
		float distance          = 14.f;
		float minDistance       = 6.f;
		float maxDistance       = 40.f;
		float zoomStep          = 2.f;
		float pitch             = 20.f;
		float minPitch          = -5.f;
		float maxPitch          = 80.f;
		float lookHeight        = 2.f;
		float followSharpness   = 6.f;
		float recenterDelay     = 2.f;
		float recenterSharpness = 1.5f;
		float recenterMinSpeed  = 2.f;
	};

	class ChaseCamera
	{
	public:
		ChaseCamera(const ChaseCameraSettings& settings) noexcept;

		void orbit(const glm::vec2& rotation, float scroll);
		void snap(lunar::Transform& camera, const lunar::Transform& target);
		void update(lunar::Transform& camera, const lunar::Transform& target, float target_speed, float delta_time);

		float getDistance() const;

	private:
		void place(lunar::Transform& camera) const;

		ChaseCameraSettings settings;
		glm::vec3           focus     = {};
		float               heading   = 0.f;
		float               yawOffset = 0.f;
		float               pitch     = 0.f;
		float               distance  = 0.f;
		float               idleTime  = 0.f;
	};
}
