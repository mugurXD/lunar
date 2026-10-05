#include <trok/vehicle/ai_vehicle.hpp>

#include <utility>

namespace trok
{
	lunar::Physics::VehicleInput ParkedController::decide(const AiVehicle&, float)
	{
		return PARKED_INPUT;
	}

	AiVehicle::AiVehicle(lunar::Scene&                      scene,
	                     lunar::Render::MeshRegistry&       meshes,
	                     const VehicleDefinition&           definition,
	                     const glm::vec3&                   position,
	                     const glm::quat&                   rotation,
	                     std::unique_ptr<VehicleController> controller) noexcept
		: Vehicle(scene, meshes, definition, position, rotation),
		controller(std::move(controller))
	{
	}

	void AiVehicle::update(float delta_time)
	{
		drive(controller->decide(*this, delta_time), delta_time);
	}

	void AiVehicle::setController(std::unique_ptr<VehicleController> new_controller)
	{
		controller = std::move(new_controller);
	}
}
