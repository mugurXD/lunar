#include <trok/vehicle/vehicle.hpp>
#include <trok/json_math.hpp>

#include <lunar/core/scene.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/render/components.hpp>

#include <algorithm>
#include <cmath>

namespace trok
{
	namespace
	{
		constexpr uint32_t VEHICLE_FORMAT_VERSION = 4;
		constexpr float    HALF                   = 0.5f;

		const glm::vec3    CHASSIS_FORWARD        = { 0.f, 0.f, -1.f };
		const glm::vec3    CHASSIS_UP             = { 0.f, 1.f, 0.f };

		nlohmann::json SerializeWheel(const lunar::Physics::WheelSettings& wheel)
		{
			return nlohmann::json
			{
				{ "position", SerializeVec3(wheel.mountPoint) },
				{ "radius",   wheel.radius },
				{ "steered",  wheel.steered },
				{ "driven",   wheel.driven }
			};
		}

		lunar::Physics::WheelSettings DeserializeWheel(const nlohmann::json& json)
		{
			return lunar::Physics::WheelSettings
			{
				.mountPoint = DeserializeVec3(json.at("position")),
				.radius     = json.at("radius").get<float>(),
				.steered    = json.at("steered").get<bool>(),
				.driven     = json.at("driven").get<bool>()
			};
		}

		nlohmann::json SerializePart(const VehiclePart& part)
		{
			return nlohmann::json
			{
				{ "halfExtents", SerializeVec3(part.halfExtents) },
				{ "offset",      SerializeVec3(part.offset) },
				{ "color",       SerializeVec3(part.color) }
			};
		}

		VehiclePart DeserializePart(const nlohmann::json& json)
		{
			return VehiclePart
			{
				.halfExtents = DeserializeVec3(json.at("halfExtents")),
				.offset      = DeserializeVec3(json.at("offset")),
				.color       = DeserializeVec3(json.at("color"))
			};
		}

		bool IsDrivable(const VehicleDefinition& definition)
		{
			return definition.mass > 0.f && std::ranges::any_of(definition.vehicle.wheels, &lunar::Physics::WheelSettings::driven);
		}
	}

	nlohmann::json VehicleDefinition::Serialize(const VehicleDefinition& definition)
	{
		const lunar::Physics::VehicleSettings& vehicle = definition.vehicle;

		nlohmann::json wheels = nlohmann::json::array();
		for (const lunar::Physics::WheelSettings& wheel : vehicle.wheels)
			wheels.push_back(SerializeWheel(wheel));

		nlohmann::json parts = nlohmann::json::array();
		for (const VehiclePart& part : definition.parts)
			parts.push_back(SerializePart(part));

		return nlohmann::json
		{
			{ "formatVersion", VEHICLE_FORMAT_VERSION },
			{ "name",          definition.name },
			{ "body",
				{
					{ "mass",                definition.mass },
					{ "centerOfMass",        SerializeVec3(definition.centerOfMass) },
					{ "colliderHalfExtents", SerializeVec3(definition.colliderHalfExtents) },
					{ "colliderOffset",      SerializeVec3(definition.colliderOffset) }
				}
			},
			{ "wheels",
				{
					{ "width",  definition.wheelWidth },
					{ "color",  SerializeVec3(definition.wheelColor) },
					{ "mounts", std::move(wheels) }
				}
			},
			{ "suspension",
				{
					{ "restLength",        vehicle.restLength },
					{ "stiffness",         vehicle.stiffness },
					{ "damping",           vehicle.damping },
					{ "antiRollStiffness", vehicle.antiRollStiffness }
				}
			},
			{ "handling",
				{
					{ "extraGravity",      vehicle.extraGravity },
					{ "downforce",         vehicle.downforce },
					{ "airControl",        vehicle.airControl },
					{ "airControlRate",    vehicle.airControlRate },
					{ "selfRighting",      vehicle.selfRighting },
					{ "selfRightingSpeed", vehicle.selfRightingSpeed }
				}
			},
			{ "engine",
				{
					{ "driveForce",        vehicle.driveForce },
					{ "power",             vehicle.enginePower },
					{ "maxSpeed",          vehicle.maxSpeed },
					{ "brakeForce",        vehicle.brakeForce },
					{ "rollingResistance", vehicle.rollingResistance },
					{ "dragCoefficient",   vehicle.dragCoefficient }
				}
			},
			{ "steering",
				{
					{ "maxAngle", vehicle.maxSteerAngle },
					{ "speed",    vehicle.steerSpeed }
				}
			},
			{ "tyres",
				{
					{ "corneringStiffness", vehicle.corneringStiffness },
					{ "friction",           vehicle.tyreFriction },
					{ "roadGrip",           vehicle.roadGrip },
					{ "rollInfluence",      vehicle.rollInfluence }
				}
			},
			{ "parts", std::move(parts) }
		};
	}

	std::optional<VehicleDefinition> VehicleDefinition::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != VEHICLE_FORMAT_VERSION)
			return std::nullopt;

		const nlohmann::json& body       = json.at("body");
		const nlohmann::json& wheels     = json.at("wheels");
		const nlohmann::json& suspension = json.at("suspension");
		const nlohmann::json& engine     = json.at("engine");
		const nlohmann::json& steering   = json.at("steering");
		const nlohmann::json& tyres      = json.at("tyres");
		const nlohmann::json& handling   = json.at("handling");

		VehicleDefinition definition =
		{
			.name                = json.at("name").get<std::string>(),
			.mass                = body.at("mass").get<float>(),
			.centerOfMass        = DeserializeVec3(body.at("centerOfMass")),
			.colliderHalfExtents = DeserializeVec3(body.at("colliderHalfExtents")),
			.colliderOffset      = DeserializeVec3(body.at("colliderOffset")),
			.wheelWidth          = wheels.at("width").get<float>(),
			.wheelColor          = DeserializeVec3(wheels.at("color")),
			.vehicle             =
			{
				.restLength         = suspension.at("restLength").get<float>(),
				.stiffness          = suspension.at("stiffness").get<float>(),
				.damping            = suspension.at("damping").get<float>(),
				.driveForce         = engine.at("driveForce").get<float>(),
				.enginePower        = engine.at("power").get<float>(),
				.maxSpeed           = engine.at("maxSpeed").get<float>(),
				.brakeForce         = engine.at("brakeForce").get<float>(),
				.maxSteerAngle      = steering.at("maxAngle").get<float>(),
				.steerSpeed         = steering.at("speed").get<float>(),
				.corneringStiffness = tyres.at("corneringStiffness").get<float>(),
				.tyreFriction       = tyres.at("friction").get<float>(),
				.roadGrip           = tyres.at("roadGrip").get<float>(),
				.rollingResistance  = engine.at("rollingResistance").get<float>(),
				.dragCoefficient    = engine.at("dragCoefficient").get<float>(),
				.rollInfluence      = tyres.at("rollInfluence").get<float>(),
				.antiRollStiffness  = suspension.at("antiRollStiffness").get<float>(),
				.extraGravity       = handling.at("extraGravity").get<float>(),
				.downforce          = handling.at("downforce").get<float>(),
				.airControl         = handling.at("airControl").get<float>(),
				.airControlRate     = handling.at("airControlRate").get<float>(),
				.selfRighting       = handling.at("selfRighting").get<float>(),
				.selfRightingSpeed  = handling.at("selfRightingSpeed").get<float>()
			}
		};

		for (const nlohmann::json& wheel : wheels.at("mounts"))
			definition.vehicle.wheels.push_back(DeserializeWheel(wheel));

		for (const nlohmann::json& part : json.at("parts"))
			definition.parts.push_back(DeserializePart(part));

		if (!IsDrivable(definition))
		{
			Fs::ReportMalformedJson("a vehicle needs a positive mass and at least one driven wheel");
			return std::nullopt;
		}

		return definition;
	}

	lunar::Physics::RigidBody CreateVehicleBody(lunar::Scene&            scene,
	                                            const VehicleDefinition& definition,
	                                            const glm::vec3&         position,
	                                            const glm::quat&         rotation)
	{
		lunar::Physics::RigidBody body(scene, position, rotation, lunar::Physics::BodyType::eDynamic);
		body.addBox(definition.colliderHalfExtents, definition.colliderOffset, lunar::Physics::VEHICLE_CATEGORY);
		body.setMass(definition.mass, definition.centerOfMass);
		body.setSleepAllowed(false);
		return body;
	}

	Vehicle::Vehicle(lunar::Scene&                scene,
	                 lunar::Render::MeshRegistry& meshes,
	                 const VehicleDefinition&     definition,
	                 const glm::vec3&             position,
	                 const glm::quat&             rotation) noexcept
		: meshes(meshes),
		chassis(scene.createGameObject(definition.name)),
		vehicle(definition.vehicle),
		wheelWidth(definition.wheelWidth)
	{
		chassis->getTransform().position = position;
		chassis->getTransform().rotation = rotation;
		chassis->addComponent<lunar::Physics::RigidBody>(CreateVehicleBody(scene, definition, position, rotation));
		chassis->addComponent<TrafficBody>(TrafficBody { .halfLength = definition.colliderHalfExtents.z });

		for (const VehiclePart& part : definition.parts)
			createBox(part.color, part.halfExtents, part.offset);

		for (const lunar::Physics::WheelSettings& wheel : definition.vehicle.wheels)
			wheels.push_back(createBox(definition.wheelColor, glm::vec3(wheelWidth * HALF, wheel.radius, wheel.radius), wheel.mountPoint));

		currentWheels.resize(wheels.size());
		captureWheels();
		captureWheels();
		updateWheels(1.f);
	}

	Vehicle::~Vehicle() noexcept
	{
		chassis->destroy();
		for (const lunar::Render::MeshHandle mesh : ownedMeshes)
			meshes.destroy(mesh);
	}

	void Vehicle::drive(const lunar::Physics::VehicleInput& input, float delta_time)
	{
		lunar::Physics::RigidBody& body = *chassis->getComponent<lunar::Physics::RigidBody>();
		if (body.isActive())
			vehicle.update(body, input, delta_time);

		captureWheels();
	}

	void Vehicle::recover(const glm::vec3& position, std::optional<glm::vec2> facing, float speed)
	{
		lunar::Physics::RigidBody& body    = *chassis->getComponent<lunar::Physics::RigidBody>();
		const glm::vec3            current = getTransform().rotation * CHASSIS_FORWARD;
		const glm::vec2            forward = facing.value_or(glm::vec2(current.x, current.z));
		const glm::quat            upright = glm::angleAxis(std::atan2(-forward.x, -forward.y), CHASSIS_UP);

		heldVelocity = upright * CHASSIS_FORWARD * speed;
		heldSpin     = glm::vec3(0.f);

		body.setTransform(position, upright);
		body.setLinearVelocity(heldVelocity);
		body.setAngularVelocity(heldSpin);

		body.capturePose();
		body.capturePose();
		captureWheels();
		captureWheels();
	}

	void Vehicle::setSimulated(bool active)
	{
		lunar::Physics::RigidBody& body = *chassis->getComponent<lunar::Physics::RigidBody>();
		simulated = active;
		if (body.isActive() == active)
			return;

		if (!active)
		{
			heldVelocity = body.getLinearVelocity();
			heldSpin     = body.getAngularVelocity();
		}

		// Sleeping bodies lose their velocity, so it is carried across the pause to keep a car from stopping dead.
		body.setActive(active);
		if (active)
		{
			body.setLinearVelocity(heldVelocity);
			body.setAngularVelocity(heldSpin);
		}
	}

	void Vehicle::updateWheels(float alpha)
	{
		for (size_t wheel = 0; wheel < wheels.size(); wheel++)
		{
			lunar::Transform& transform = wheels[wheel]->getTransform();
			transform.position = glm::mix(previousWheels[wheel].position, currentWheels[wheel].position, alpha);
			transform.rotation = glm::slerp(previousWheels[wheel].rotation, currentWheels[wheel].rotation, alpha);
		}
	}

	void Vehicle::captureWheels()
	{
		previousWheels = currentWheels;
		for (size_t wheel = 0; wheel < currentWheels.size(); wheel++)
			currentWheels[wheel] = { vehicle.getWheelLocalPosition(wheel), vehicle.getWheelLocalRotation(wheel) };
	}

	const lunar::Transform& Vehicle::getTransform() const
	{
		return chassis->getTransform();
	}

	glm::vec3 Vehicle::getVelocity()
	{
		return chassis->getComponent<lunar::Physics::RigidBody>()->getLinearVelocity();
	}

	glm::vec3 Vehicle::getUp()
	{
		return chassis->getComponent<lunar::Physics::RigidBody>()->getRotation() * CHASSIS_UP;
	}

	const lunar::Physics::RaycastVehicle& Vehicle::getVehicle() const
	{
		return vehicle;
	}

	lunar::Physics::RaycastVehicle& Vehicle::editVehicle()
	{
		return vehicle;
	}

	lunar::Entity Vehicle::getEntity() const
	{
		return chassis.getEntity();
	}

	bool Vehicle::isSimulated() const
	{
		return simulated;
	}

	lunar::GameObject Vehicle::createBox(const glm::vec3& color, const glm::vec3& half_extents, const glm::vec3& offset)
	{
		lunar::Render::MeshData data = lunar::Render::CreateCubeMeshData();
		for (lunar::Render::Vertex& vertex : data.vertices)
			vertex.color = glm::vec4(color, 1.f);

		const lunar::Render::MeshHandle mesh = meshes.create(data);
		ownedMeshes.push_back(mesh);

		lunar::GameObject box = chassis->createChildObject(chassis->getName());
		box->getTransform().position = offset;
		box->getTransform().scale    = half_extents;
		box->addComponent<lunar::MeshRenderer>(mesh);
		return box;
	}
}
