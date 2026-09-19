#pragma once
#include <lunar/api.hpp>
#include <lunar/physics/rigid_body.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace lunar::Physics
{
	struct LUNAR_API WheelSettings
	{
		glm::vec3 mountPoint = {};
		float     radius     = 0.5f;
		bool      steered    = false;
		bool      driven     = false;

		bool operator==(const WheelSettings&) const = default;
	};

	struct LUNAR_API VehicleSettings
	{
		std::vector<WheelSettings> wheels             = {};
		float                      restLength         = 0.5f;
		float                      stiffness          = 0.f;
		float                      damping            = 0.f;
		float                      driveForce         = 0.f;
		float                      enginePower        = std::numeric_limits<float>::infinity();
		float                      maxSpeed           = std::numeric_limits<float>::infinity();
		float                      brakeForce         = 0.f;
		float                      maxSteerAngle      = 30.f;
		float                      steerSpeed         = 60.f;
		float                      corneringStiffness = 0.f;
		float                      tyreFriction       = 1.f;
		float                      rollingResistance  = 0.f;
		float                      dragCoefficient    = 0.f;
		float                      rollInfluence      = 0.3f;

		bool operator==(const VehicleSettings&) const = default;
	};

	struct LUNAR_API VehicleInput
	{
		float throttle = 0.f;
		float brake    = 0.f;
		float steering = 0.f;
	};

	struct LUNAR_API WheelState
	{
		bool  grounded    = false;
		float compression = 0.f;
		float spinAngle   = 0.f;
	};

	class LUNAR_API RaycastVehicle
	{
	public:
		RaycastVehicle(VehicleSettings settings) noexcept;

		void update(RigidBody& chassis, const VehicleInput& input, float delta_time);

		const VehicleSettings&      getSettings()                        const;
		VehicleSettings&            editSettings();
		std::span<const WheelState> getWheels()                          const;
		float                       getForwardSpeed()                    const;
		float                       getSteerAngle()                      const;
		glm::vec3                   getWheelLocalPosition(size_t wheel) const;
		glm::quat                   getWheelLocalRotation(size_t wheel) const;

	private:
		struct ChassisState
		{
			glm::vec3 position        = {};
			glm::quat rotation        = {};
			glm::vec3 up              = {};
			glm::vec3 forward         = {};
			glm::vec3 centerOfMass    = {};
			glm::vec3 linearVelocity  = {};
			glm::vec3 angularVelocity = {};
			float     massPerWheel    = 0.f;
			size_t    drivenWheels    = 0;
		};

		void  updateSteering(float steering, float delta_time);
		float totalDriveForce(float throttle) const;
		void updateWheel(size_t wheel, RigidBody& chassis, const ChassisState& state, const VehicleInput& input, float delta_time);

		VehicleSettings         settings     = {};
		std::vector<WheelState> wheels       = {};
		float                   forwardSpeed = 0.f;
		float                   steerAngle   = 0.f;
	};
}
