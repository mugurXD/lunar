#include <lunar/physics/raycast_vehicle.hpp>
#include <lunar/physics/conversions.hpp>
#include <lunar/physics/raycast.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace lunar::Physics
{
	namespace
	{
		const glm::vec3 LOCAL_UP      = { 0.f, 1.f, 0.f };
		const glm::vec3 LOCAL_FORWARD = { 0.f, 0.f, -1.f };
		const glm::vec3 LOCAL_RIGHT   = { 1.f, 0.f, 0.f };
		const glm::vec3 WORLD_UP      = { 0.f, 1.f, 0.f };

		constexpr uint16_t WHEEL_RAY_MASK  = static_cast<uint16_t>(~VEHICLE_CATEGORY);
		constexpr float    MIN_POWER_SPEED = 1.f;
		constexpr float    AXLE_TOLERANCE  = 0.01f;

		glm::vec3 ProjectOntoPlane(const glm::vec3& vector, const glm::vec3& normal)
		{
			return glm::normalize(vector - normal * glm::dot(vector, normal));
		}

		std::optional<size_t> AxlePartner(std::span<const WheelSettings> wheels, size_t wheel)
		{
			const glm::vec3& mount = wheels[wheel].mountPoint;
			for (size_t other = 0; other < wheels.size(); other++)
			{
				const glm::vec3& candidate = wheels[other].mountPoint;
				if (std::abs(candidate.z - mount.z) <= AXLE_TOLERANCE && candidate.x * mount.x < 0.f)
					return other;
			}

			return std::nullopt;
		}
	}

	RaycastVehicle::RaycastVehicle(VehicleSettings settings) noexcept
		: settings(std::move(settings)),
		wheels(this->settings.wheels.size()),
		contacts(this->settings.wheels.size())
	{
	}

	void RaycastVehicle::update(RigidBody& chassis, const VehicleInput& input, float delta_time)
	{
		rp3d::RigidBody&       body      = chassis.getBody();
		const rp3d::Transform& transform = body.getTransform();

		ChassisState state =
		{
			.position        = ToGlm(transform.getPosition()),
			.rotation        = ToGlm(transform.getOrientation()),
			.centerOfMass    = ToGlm(body.getWorldPoint(body.getLocalCenterOfMass())),
			.linearVelocity  = ToGlm(body.getLinearVelocity()),
			.angularVelocity = ToGlm(body.getAngularVelocity()),
			.massPerWheel    = body.getMass() / static_cast<float>(std::max<size_t>(wheels.size(), 1)),
			.drivenWheels    = static_cast<size_t>(std::ranges::count(settings.wheels, true, &WheelSettings::driven))
		};

		state.up      = state.rotation * LOCAL_UP;
		state.forward = state.rotation * LOCAL_FORWARD;
		forwardSpeed  = glm::dot(state.linearVelocity, state.forward);

		updateSteering(input.steering, delta_time);

		for (size_t wheel = 0; wheel < wheels.size(); wheel++)
			contacts[wheel] = probeWheel(wheel, chassis, state, delta_time);

		for (size_t wheel = 0; wheel < wheels.size(); wheel++)
			if (contacts[wheel].has_value())
				applyWheel(wheel, *contacts[wheel], chassis, state, input, delta_time);

		const bool      grounded  = std::ranges::any_of(wheels, &WheelState::grounded);
		const glm::vec3 drag      = -state.linearVelocity * glm::length(state.linearVelocity) * settings.dragCoefficient;
		const glm::vec3 gravity   = -WORLD_UP * body.getMass() * settings.extraGravity;
		const glm::vec3 downforce = grounded ? -state.up * settings.downforce * forwardSpeed * forwardSpeed : glm::vec3(0.f);
		body.applyWorldForceAtCenterOfMass(ToPhysics(drag + gravity + downforce));
	}

	const VehicleSettings& RaycastVehicle::getSettings() const
	{
		return settings;
	}

	VehicleSettings& RaycastVehicle::editSettings()
	{
		return settings;
	}

	std::span<const WheelState> RaycastVehicle::getWheels() const
	{
		return wheels;
	}

	float RaycastVehicle::getForwardSpeed() const
	{
		return forwardSpeed;
	}

	float RaycastVehicle::getSteerAngle() const
	{
		return steerAngle;
	}

	glm::vec3 RaycastVehicle::getWheelLocalPosition(size_t wheel) const
	{
		const float suspension_length = settings.restLength - wheels[wheel].compression;
		return settings.wheels[wheel].mountPoint - LOCAL_UP * suspension_length;
	}

	glm::quat RaycastVehicle::getWheelLocalRotation(size_t wheel) const
	{
		const float     steer = settings.wheels[wheel].steered ? steerAngle : 0.f;
		const glm::quat spin  = glm::angleAxis(-wheels[wheel].spinAngle, LOCAL_RIGHT);
		return glm::angleAxis(-steer, LOCAL_UP) * spin;
	}

	void RaycastVehicle::updateSteering(float steering, float delta_time)
	{
		const float target   = glm::radians(settings.maxSteerAngle) * std::clamp(steering, -1.f, 1.f);
		const float max_step = glm::radians(settings.steerSpeed) * delta_time;
		steerAngle += std::clamp(target - steerAngle, -max_step, max_step);
	}

	float RaycastVehicle::totalDriveForce(float throttle) const
	{
		const float speed = std::abs(forwardSpeed);
		if (speed >= settings.maxSpeed && throttle * forwardSpeed > 0.f)
			return 0.f;

		const float available = std::min(settings.driveForce, settings.enginePower / std::max(speed, MIN_POWER_SPEED));
		return std::clamp(throttle, -1.f, 1.f) * available;
	}

	float RaycastVehicle::antiRollLoad(size_t wheel) const
	{
		const std::optional<size_t> partner = AxlePartner(settings.wheels, wheel);
		return partner.has_value() ? settings.antiRollStiffness * (wheels[wheel].compression - wheels[*partner].compression) : 0.f;
	}

	std::optional<RaycastVehicle::Contact> RaycastVehicle::probeWheel(size_t wheel, RigidBody& chassis, const ChassisState& state, float delta_time)
	{
		const WheelSettings& wheel_settings = settings.wheels[wheel];
		WheelState&          wheel_state    = wheels[wheel];
		const glm::vec3      mount          = state.position + state.rotation * wheel_settings.mountPoint;
		const float          ray_length     = settings.restLength + wheel_settings.radius;

		const std::optional<RaycastHit> hit                  = CastRay(chassis.getWorld(), mount, mount - state.up * ray_length, WHEEL_RAY_MASK);
		const float                     previous_compression = wheel_state.compression;

		wheel_state.grounded    = hit.has_value();
		wheel_state.compression = hit.has_value() ? std::max(settings.restLength - (hit->fraction * ray_length - wheel_settings.radius), 0.f) : 0.f;
		wheel_state.spinAngle  += forwardSpeed / wheel_settings.radius * delta_time;
		if (!hit.has_value())
			return std::nullopt;

		return Contact { .hit = *hit, .compressionSpeed = (wheel_state.compression - previous_compression) / delta_time };
	}

	void RaycastVehicle::applyWheel(size_t wheel, const Contact& contact, RigidBody& chassis, const ChassisState& state, const VehicleInput& input, float delta_time)
	{
		const WheelSettings& wheel_settings = settings.wheels[wheel];
		const RaycastHit&    hit            = contact.hit;
		const float          spring         = settings.stiffness * wheels[wheel].compression + settings.damping * contact.compressionSpeed;
		const float          load           = std::max(spring + antiRollLoad(wheel), 0.f);

		const float     steer          = wheel_settings.steered ? steerAngle : 0.f;
		const glm::vec3 wheel_forward  = ProjectOntoPlane(state.rotation * (glm::angleAxis(-steer, LOCAL_UP) * LOCAL_FORWARD), hit.normal);
		const glm::vec3 wheel_side     = glm::normalize(glm::cross(wheel_forward, hit.normal));
		const glm::vec3 point_velocity = state.linearVelocity + glm::cross(state.angularVelocity, hit.point - state.centerOfMass);
		const float     forward_slip   = glm::dot(point_velocity, wheel_forward);
		const float     side_slip      = glm::dot(point_velocity, wheel_side);
		const float     stopping_force = state.massPerWheel / delta_time;
		const float     brake_limit    = input.brake * settings.brakeForce / static_cast<float>(wheels.size());

		float longitudinal = -forward_slip * settings.rollingResistance / static_cast<float>(wheels.size());
		if (wheel_settings.driven && state.drivenWheels > 0)
			longitudinal += totalDriveForce(input.throttle) / static_cast<float>(state.drivenWheels);

		longitudinal -= std::clamp(forward_slip * stopping_force, -brake_limit, brake_limit);

		const float lateral   = -side_slip * std::min(settings.corneringStiffness, stopping_force);
		glm::vec3   traction  = wheel_forward * longitudinal + wheel_side * lateral;
		const float surface   = (hit.category & ROAD_CATEGORY) != 0 ? settings.roadGrip : 1.f;
		const float max_grip  = settings.tyreFriction * surface * load;
		const float magnitude = glm::length(traction);
		if (magnitude > max_grip)
			traction *= max_grip / magnitude;

		const float     height_to_mass = glm::dot(state.centerOfMass - hit.point, state.up);
		const glm::vec3 traction_point = hit.point + state.up * height_to_mass * (1.f - settings.rollInfluence);

		rp3d::RigidBody& body = chassis.getBody();
		body.applyWorldForceAtWorldPosition(ToPhysics(state.up * load), ToPhysics(hit.point));
		body.applyWorldForceAtWorldPosition(ToPhysics(traction), ToPhysics(traction_point));
	}
}
