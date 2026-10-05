#pragma once
#include <trok/vehicle/vehicle.hpp>

#include <memory>

namespace trok
{
	class AiVehicle;

	class VehicleController
	{
	public:
		virtual ~VehicleController() = default;

		virtual lunar::Physics::VehicleInput decide(const AiVehicle& vehicle, float delta_time) = 0;
	};

	class ParkedController : public VehicleController
	{
	public:
		lunar::Physics::VehicleInput decide(const AiVehicle& vehicle, float delta_time) override;
	};

	class AiVehicle : public Vehicle
	{
	public:
		AiVehicle(lunar::Scene&                      scene,
		          lunar::Render::MeshRegistry&       meshes,
		          const VehicleDefinition&           definition,
		          const glm::vec3&                   position,
		          const glm::quat&                   rotation,
		          std::unique_ptr<VehicleController> controller) noexcept;

		void update(float delta_time);
		void setController(std::unique_ptr<VehicleController> controller);

	private:
		std::unique_ptr<VehicleController> controller;
	};
}
