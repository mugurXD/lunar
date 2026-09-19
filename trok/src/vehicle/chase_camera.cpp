#include <trok/vehicle/chase_camera.hpp>

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace trok
{
	namespace
	{
		constexpr float MIN_HEADING_LENGTH = 1e-3f;

		const glm::vec3 WORLD_UP      = { 0.f, 1.f, 0.f };
		const glm::vec3 LOCAL_FORWARD = { 0.f, 0.f, -1.f };

		glm::vec3 FlatHeading(const glm::quat& rotation)
		{
			const glm::vec3 forward = rotation * LOCAL_FORWARD;
			const glm::vec3 flat    = glm::vec3(forward.x, 0.f, forward.z);
			return glm::length(flat) > MIN_HEADING_LENGTH ? glm::normalize(flat) : LOCAL_FORWARD;
		}
	}

	glm::vec3 ChaseCameraPosition(const lunar::Transform& target, const ChaseCameraSettings& settings)
	{
		return target.position - FlatHeading(target.rotation) * settings.distance + WORLD_UP * settings.height;
	}

	void FollowTarget(lunar::Transform& camera, const lunar::Transform& target, const ChaseCameraSettings& settings, float delta_time)
	{
		const float     blend = 1.f - std::exp(-settings.sharpness * delta_time);
		const glm::vec3 focus = target.position + WORLD_UP * settings.lookHeight;

		camera.position = glm::mix(camera.position, ChaseCameraPosition(target, settings), blend);
		camera.rotation = glm::quatLookAt(glm::normalize(focus - camera.position), WORLD_UP);
	}
}
