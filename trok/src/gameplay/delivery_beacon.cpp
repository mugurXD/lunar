#include <trok/gameplay/delivery_beacon.hpp>

#include <lunar/core/scene.hpp>
#include <lunar/render/components.hpp>

#include <algorithm>
#include <string_view>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr std::string_view BEACON_NAME = "DeliveryBeacon";
		constexpr float            HALF        = 0.5f;

		lunar::Render::MeshHandle CreateBeaconMesh(lunar::Render::MeshRegistry& meshes, const glm::vec4& color)
		{
			lunar::Render::MeshData data = lunar::Render::CreateCubeMeshData();
			for (lunar::Render::Vertex& vertex : data.vertices)
				vertex.color = color;

			return meshes.create(data);
		}
	}

	DeliveryBeacon::DeliveryBeacon(lunar::Scene& scene, lunar::Render::MeshRegistry& meshes, lunar::Render::MaterialHandle material, BeaconSettings settings)
		: meshes(meshes),
		mesh(CreateBeaconMesh(meshes, settings.color)),
		beacon(scene.createGameObject(BEACON_NAME)),
		settings(std::move(settings))
	{
		beacon->addComponent<lunar::MeshRenderer>(mesh, false, true, material);
	}

	DeliveryBeacon::~DeliveryBeacon() noexcept
	{
		beacon->destroy();
		meshes.destroy(mesh);
	}

	void DeliveryBeacon::update(const glm::vec3& eye, const std::optional<glm::dvec2>& target)
	{
		lunar::MeshRenderer* renderer = beacon->getComponent<lunar::MeshRenderer>();
		renderer->visible = target.has_value();
		if (!target.has_value())
			return;

		const glm::vec3 spot       = beacon->getScene()->toScene({ target->x, 0.0, target->y });
		const float     bottom     = eye.y - settings.depthBelowEye;
		const float     top        = eye.y + settings.heightAboveEye;
		const float     distance   = glm::distance(glm::vec2(eye.x, eye.z), glm::vec2(spot.x, spot.z));
		const float     half_width = settings.halfWidth * std::max(1.f, distance / settings.constantWidthDistance);

		beacon->getTransform().position = { spot.x, (bottom + top) * HALF, spot.z };
		beacon->getTransform().scale    = { half_width, (top - bottom) * HALF, half_width };
	}
}
