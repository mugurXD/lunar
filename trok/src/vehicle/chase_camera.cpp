#include <trok/vehicle/chase_camera.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr float FULL_TURN_DEGREES = 360.f;
		constexpr float FULL_TURN_RADIANS = glm::two_pi<float>();

		const glm::vec3 WORLD_UP      = { 0.f, 1.f, 0.f };
		const glm::vec3 LOCAL_FORWARD = { 0.f, 0.f, -1.f };

		float Heading(const glm::quat& rotation)
		{
			const glm::vec3 forward = rotation * LOCAL_FORWARD;
			return std::atan2(forward.x, -forward.z);
		}

		glm::vec3 Focus(const lunar::Transform& target, const ChaseCameraSettings& settings)
		{
			return target.position + WORLD_UP * settings.lookHeight;
		}

		float Smoothing(float sharpness, float delta_time)
		{
			return 1.f - std::exp(-sharpness * delta_time);
		}
	}

	ChaseCamera::ChaseCamera(const ChaseCameraSettings& settings) noexcept
		: settings(settings),
		pitch(settings.pitch),
		distance(settings.distance)
	{
	}

	void ChaseCamera::orbit(const glm::vec2& rotation, float scroll)
	{
		if (rotation != glm::vec2(0.f))
			idleTime = 0.f;

		yawOffset = std::remainder(yawOffset + rotation.x, FULL_TURN_DEGREES);
		pitch     = std::clamp(pitch - rotation.y, settings.minPitch, settings.maxPitch);
		distance  = std::clamp(distance - scroll * settings.zoomStep, settings.minDistance, settings.maxDistance);
	}

	void ChaseCamera::snap(lunar::Transform& camera, const lunar::Transform& target)
	{
		focus   = Focus(target, settings);
		heading = Heading(target.rotation);
		place(camera);
	}

	void ChaseCamera::update(lunar::Transform& camera, const lunar::Transform& target, float target_speed, float delta_time)
	{
		const float follow = Smoothing(settings.followSharpness, delta_time);

		focus     = glm::mix(focus, Focus(target, settings), follow);
		heading  += std::remainder(Heading(target.rotation) - heading, FULL_TURN_RADIANS) * follow;
		idleTime += delta_time;

		if (idleTime >= settings.recenterDelay && std::abs(target_speed) >= settings.recenterMinSpeed)
		{
			const float recenter = Smoothing(settings.recenterSharpness, delta_time);
			yawOffset = glm::mix(yawOffset, 0.f, recenter);
			pitch     = glm::mix(pitch, settings.pitch, recenter);
		}

		place(camera);
	}

	float ChaseCamera::getDistance() const
	{
		return distance;
	}

	void ChaseCamera::place(lunar::Transform& camera) const
	{
		const float     yaw       = heading + glm::radians(yawOffset);
		const float     elevation = glm::radians(pitch);
		const glm::vec3 behind    = -glm::vec3(std::sin(yaw), 0.f, -std::cos(yaw));
		const glm::vec3 to_camera = behind * std::cos(elevation) + WORLD_UP * std::sin(elevation);

		camera.position = focus + to_camera * distance;
		camera.rotation = glm::quatLookAt(-to_camera, WORLD_UP);
	}
}
