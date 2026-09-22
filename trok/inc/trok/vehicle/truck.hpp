#pragma once
#include <lunar/core/gameobject.hpp>
#include <lunar/physics/raycast_vehicle.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace lunar
{
	class Scene;
}

namespace trok
{
	struct TruckPart
	{
		glm::vec3 halfExtents = {};
		glm::vec3 offset      = {};
		glm::vec3 color       = {};

		bool operator==(const TruckPart&) const = default;
	};

	struct TruckDefinition
	{
		std::string                     name                = {};
		float                           mass                = 0.f;
		glm::vec3                       centerOfMass        = {};
		glm::vec3                       colliderHalfExtents = {};
		glm::vec3                       colliderOffset      = {};
		float                           wheelWidth          = 0.f;
		glm::vec3                       wheelColor          = {};
		lunar::Physics::VehicleSettings vehicle             = {};
		std::vector<TruckPart>          parts               = {};

		bool operator==(const TruckDefinition&) const = default;

		static nlohmann::json                 Serialize(const TruckDefinition& definition);
		static std::optional<TruckDefinition> Deserialize(const nlohmann::json& json);
	};

	lunar::Physics::RigidBody CreateTruckBody(lunar::Scene&          scene,
	                                          const TruckDefinition& definition,
	                                          const glm::vec3&       position,
	                                          const glm::quat&       rotation);

	class Truck
	{
	public:
		Truck(lunar::Scene&                scene,
		      lunar::Render::MeshRegistry& meshes,
		      const TruckDefinition&       definition,
		      const glm::vec3&             position,
		      const glm::quat&             rotation) noexcept;
		~Truck() noexcept;

		Truck(const Truck&)            = delete;
		Truck& operator=(const Truck&) = delete;

		void drive(const lunar::Physics::VehicleInput& input, float delta_time);
		void recover(const glm::vec3& position);
		void setSimulated(bool simulated);
		void updateWheels();

		const lunar::Transform&               getTransform()  const;
		const lunar::Physics::RaycastVehicle& getVehicle()    const;
		lunar::Physics::RaycastVehicle&       editVehicle();
		bool                                  isSimulated()   const;

	private:
		lunar::GameObject createBox(const glm::vec3& color, const glm::vec3& half_extents, const glm::vec3& offset);

		lunar::Render::MeshRegistry&           meshes;
		lunar::GameObject                      chassis;
		std::vector<lunar::GameObject>         wheels      = {};
		std::vector<lunar::Render::MeshHandle> ownedMeshes = {};
		lunar::Physics::RaycastVehicle         vehicle;
		float                                  wheelWidth  = 0.f;
		bool                                   simulated   = true;
	};
}
