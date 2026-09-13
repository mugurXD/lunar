#include <lunar/render/components.hpp>
#include <lunar/core/scene.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace lunar
{
	const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

	glm::mat4 Camera::getViewMatrix() const
	{
		return view;
	}

	glm::mat4 Camera::getProjectionMatrix(int renderWidth, int renderHeight) const
	{
		return glm::perspective(
			glm::radians(fov),
			static_cast<float>(renderWidth) / static_cast<float>(renderHeight),
			nearPlane,
			farPlane
		);
	}

	void UpdateCameras(Scene& scene, const FrameTime&)
	{
		scene.forEach<Camera>([&](Entity entity, Camera& camera) {
			const Transform* transform = scene.getComponent<Transform>(entity);
			if (transform == nullptr)
				return;

			const float yaw   = glm::radians(transform->rotation.x);
			const float pitch = glm::radians(transform->rotation.y);

			camera.front    = glm::normalize(glm::vec3(glm::cos(yaw) * glm::cos(pitch), glm::sin(pitch), glm::sin(yaw) * glm::cos(pitch)));
			camera.right    = glm::normalize(glm::cross(camera.front, WORLD_UP));
			camera.up       = glm::normalize(glm::cross(camera.right, camera.front));
			camera.position = glm::vec3(scene.resolveWorldTransform(entity).matrix[3]);
			camera.view     = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
		});
	}
}
