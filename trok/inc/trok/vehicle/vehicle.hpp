#pragma once
#include <lunar/core/gameobject.hpp>
#include <lunar/physics/raycast_vehicle.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <vector>

namespace lunar
{
	class Scene;
}

namespace trok
{
	struct VehiclePart
	{
		glm::vec3 halfExtents = {};
		glm::vec3 offset      = {};
		glm::vec3 color       = {};

		bool operator==(const VehiclePart&) const = default;
	};

	struct VehicleDefinition
	{
		std::string                     name                = {};
		float                           mass                = 0.f;
		glm::vec3                       centerOfMass        = {};
		glm::vec3                       colliderHalfExtents = {};
		glm::vec3                       colliderOffset      = {};
		float                           wheelWidth          = 0.f;
		glm::vec3                       wheelColor          = {};
		lunar::Physics::VehicleSettings vehicle             = {};
		std::vector<VehiclePart>        parts               = {};

		bool operator==(const VehicleDefinition&) const = default;

		static nlohmann::json                   Serialize(const VehicleDefinition& definition);
		static std::optional<VehicleDefinition> Deserialize(const nlohmann::json& json);
	};

	struct TrafficBody
	{
		float halfLength = 0.f;
	};

	inline const lunar::Physics::VehicleInput PARKED_INPUT = { .brake = 1.f };

	lunar::Physics::RigidBody CreateVehicleBody(lunar::Scene&            scene,
	                                            const VehicleDefinition& definition,
	                                            const glm::vec3&         position,
	                                            const glm::quat&         rotation);

	class Vehicle
	{
	public:
		Vehicle(lunar::Scene&                scene,
		        lunar::Render::MeshRegistry& meshes,
		        const VehicleDefinition&     definition,
		        const glm::vec3&             position,
		        const glm::quat&             rotation) noexcept;
		virtual ~Vehicle() noexcept;

		Vehicle(const Vehicle&)            = delete;
		Vehicle& operator=(const Vehicle&) = delete;

		void drive(const lunar::Physics::VehicleInput& input, float delta_time);
		void recover(const glm::vec3& position, std::optional<glm::vec2> facing = std::nullopt, float speed = 0.f);
		void setSimulated(bool simulated);
		void updateWheels(float alpha);

		const lunar::Transform&               getTransform()  const;
		const lunar::Physics::RaycastVehicle& getVehicle()    const;
		lunar::Physics::RaycastVehicle&       editVehicle();
		glm::vec3                             getVelocity();
		glm::vec3                             getUp();
		lunar::Entity                         getEntity()     const;
		bool                                  isSimulated()   const;

	private:
		struct WheelPose
		{
			glm::vec3 position = {};
			glm::quat rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		};

		lunar::GameObject createBox(const glm::vec3& color, const glm::vec3& half_extents, const glm::vec3& offset);
		void              captureWheels();

		lunar::Render::MeshRegistry&           meshes;
		lunar::GameObject                      chassis;
		std::vector<lunar::GameObject>         wheels         = {};
		std::vector<WheelPose>                 previousWheels = {};
		std::vector<WheelPose>                 currentWheels  = {};
		std::vector<lunar::Render::MeshHandle> ownedMeshes = {};
		lunar::Physics::RaycastVehicle         vehicle;
		float                                  wheelWidth   = 0.f;
		glm::vec3                              heldVelocity = {};
		glm::vec3                              heldSpin     = {};
		bool                                   simulated    = true;
	};
}
