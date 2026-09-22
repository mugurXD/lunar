#pragma once
#include <lunar/api.hpp>
#include <lunar/core/time.hpp>
#include <lunar/render/gpu_types.hpp>
#include <glm/glm.hpp>

namespace lunar
{
	class LUNAR_API Scene;

	struct LUNAR_API Camera
	{
		float     fov       = 60.f;
		float     nearPlane = 0.1f;
		float     farPlane  = 1000000.f;
		glm::vec3 front     = { 0.f, 0.f, -1.f };
		glm::vec3 right     = { 1.f, 0.f, 0.f };
		glm::vec3 up        = { 0.f, 1.f, 0.f };
		glm::vec3 position  = { 0.f, 0.f, 0.f };
		glm::mat4 view      = glm::mat4(1.f);

		glm::mat4 getViewMatrix()                                       const;
		glm::mat4 getProjectionMatrix(int renderWidth, int renderHeight) const;
	};

	struct LUNAR_API MeshRenderer
	{
		Render::MeshHandle mesh        = {};
		bool               visible     = true;
		bool               translucent = false;
	};

	struct LUNAR_API DirectionalLight
	{
		glm::vec3 direction = { 0.f, -1.f, 0.f };
		glm::vec3 color     = { 1.f, 1.f, 1.f };
		float     intensity = 1.f;
	};

	struct LUNAR_API DistanceFog
	{
		glm::vec3 color         = { 0.6f, 0.745f, 0.76f };
		float     startDistance = 0.f;
		float     endDistance   = 0.f;
	};

	LUNAR_API glm::quat CameraRotation(float yaw_degrees, float pitch_degrees);
	LUNAR_API void      UpdateCameras(Scene& scene, const FrameTime& frame_time);
}
