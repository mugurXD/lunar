#include <lunar/render/components.hpp>
#include <lunar/core/scene.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lunar
{
	const glm::vec3 WORLD_UP       = { 0.f, 1.f, 0.f };
	const glm::vec3 CAMERA_FORWARD = { 0.f, 0.f, -1.f };

	glm::mat4 Camera::getViewMatrix() const
	{
		return view;
	}

	glm::mat4 Camera::getProjectionMatrix(int renderWidth, int renderHeight) const
	{
		const float aspect_ratio = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
		const float focal_length = 1.f / glm::tan(glm::radians(fov) * 0.5f);

		glm::mat4 projection = glm::mat4(0.f);
		projection[0][0] = focal_length / aspect_ratio;
		projection[1][1] = focal_length;
		projection[2][3] = -1.f;
		projection[3][2] = nearPlane;
		return projection;
	}

	glm::quat CameraRotation(float yaw_degrees, float pitch_degrees)
	{
		const float     yaw   = glm::radians(yaw_degrees);
		const float     pitch = glm::radians(pitch_degrees);
		const glm::vec3 front = glm::vec3(glm::cos(yaw) * glm::cos(pitch), glm::sin(pitch), glm::sin(yaw) * glm::cos(pitch));

		return glm::quatLookAt(glm::normalize(front), WORLD_UP);
	}

	void UpdateCameras(Scene& scene, const FrameTime&)
	{
		scene.forEach<Camera>([&](Entity entity, Camera& camera) {
			const Transform* transform = scene.getComponent<Transform>(entity);
			if (transform == nullptr)
				return;

			camera.front    = glm::normalize(transform->rotation * CAMERA_FORWARD);
			camera.right    = glm::normalize(glm::cross(camera.front, WORLD_UP));
			camera.up       = glm::normalize(glm::cross(camera.right, camera.front));
			camera.position = glm::vec3(scene.resolveWorldTransform(entity).matrix[3]);
			camera.view     = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
		});
	}
}
