#pragma once
#include <lunar/core/gameobject.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <optional>

namespace lunar
{
	class Scene;
}

namespace trok
{
	struct BeaconSettings
	{
		float     halfWidth     = 8.f;
		float     bottom        = -60.f;
		float     top           = 900.f;
		float     proxyDistance = 800.f;
		glm::vec4 color         = { 1.f, 0.55f, 0.1f, 0.6f };
	};

	class DeliveryBeacon
	{
	public:
		DeliveryBeacon(lunar::Scene& scene, lunar::Render::MeshRegistry& meshes, BeaconSettings settings = {});
		~DeliveryBeacon() noexcept;

		DeliveryBeacon(const DeliveryBeacon&)            = delete;
		DeliveryBeacon& operator=(const DeliveryBeacon&) = delete;

		void update(const glm::vec3& eye, const std::optional<glm::dvec2>& target);

	private:
		lunar::Render::MeshRegistry& meshes;
		lunar::Render::MeshHandle    mesh;
		lunar::GameObject            beacon;
		BeaconSettings               settings;
	};
}
