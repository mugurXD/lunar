#pragma once
#include <lunar/api.hpp>
#include <lunar/physics/raycast.hpp>
#include <lunar/physics/rigid_body.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstddef>
#include <limits>
#include <optional>
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
		float                      roadGrip           = 1.f;
		float                      rollingResistance  = 0.f;
		float                      dragCoefficient    = 0.f;
		float                      rollInfluence      = 0.3f;
		float                      antiRollStiffness  = 0.f;
		float                      extraGravity       = 0.f;
		float                      downforce          = 0.f;
		float                      airControl         = 0.f;
		float                      airControlRate     = 0.f;
		float                      selfRighting       = 0.f;
		float                      selfRightingSpeed  = 0.f;

		bool operator==(const VehicleSettings&) const = default;
	};

	struct LUNAR_API VehicleInput
	{
		float throttle = 0.f;
		float brake    = 0.f;
		float steering = 0.f;
		float pitch    = 0.f;
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

		struct Contact
		{
			RaycastHit hit              = {};
			float      compressionSpeed = 0.f;
		};

		void                   updateSteering(float steering, float delta_time);
		float                  totalDriveForce(float throttle)                                                             const;
		float                  antiRollLoad(size_t wheel)                                                                  const;
		std::optional<Contact> probeWheel(size_t wheel, RigidBody& chassis, const ChassisState& state, float delta_time);
		void                   applyWheel(size_t wheel, const Contact& contact, RigidBody& chassis, const ChassisState& state, const VehicleInput& input, float delta_time);
		void                   applyAirControl(rp3d::RigidBody& body, const ChassisState& state, const VehicleInput& input, float delta_time) const;
		void                   applySelfRighting(rp3d::RigidBody& body, const ChassisState& state, const VehicleInput& input) const;

		VehicleSettings                     settings     = {};
		std::vector<WheelState>             wheels       = {};
		std::vector<std::optional<Contact>> contacts     = {};
		float                   forwardSpeed = 0.f;
		float                   steerAngle   = 0.f;
	};
}
