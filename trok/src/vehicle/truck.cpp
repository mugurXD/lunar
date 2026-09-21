#include <trok/vehicle/truck.hpp>
#include <trok/json_math.hpp>

#include <lunar/core/scene.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/render/components.hpp>

#include <algorithm>

namespace trok
{
	namespace
	{
		constexpr uint32_t TRUCK_FORMAT_VERSION = 1;
		constexpr float    HALF                 = 0.5f;

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

		nlohmann::json SerializePart(const TruckPart& part)
		{
			return nlohmann::json
			{
				{ "halfExtents", SerializeVec3(part.halfExtents) },
				{ "offset",      SerializeVec3(part.offset) },
				{ "color",       SerializeVec3(part.color) }
			};
		}

		TruckPart DeserializePart(const nlohmann::json& json)
		{
			return TruckPart
			{
				.halfExtents = DeserializeVec3(json.at("halfExtents")),
				.offset      = DeserializeVec3(json.at("offset")),
				.color       = DeserializeVec3(json.at("color"))
			};
		}

		bool IsDrivable(const TruckDefinition& definition)
		{
			return definition.mass > 0.f && std::ranges::any_of(definition.vehicle.wheels, &lunar::Physics::WheelSettings::driven);
		}
	}

	nlohmann::json TruckDefinition::Serialize(const TruckDefinition& definition)
	{
		const lunar::Physics::VehicleSettings& vehicle = definition.vehicle;

		nlohmann::json wheels = nlohmann::json::array();
		for (const lunar::Physics::WheelSettings& wheel : vehicle.wheels)
			wheels.push_back(SerializeWheel(wheel));

		nlohmann::json parts = nlohmann::json::array();
		for (const TruckPart& part : definition.parts)
			parts.push_back(SerializePart(part));

		return nlohmann::json
		{
			{ "formatVersion", TRUCK_FORMAT_VERSION },
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
					{ "restLength", vehicle.restLength },
					{ "stiffness",  vehicle.stiffness },
					{ "damping",    vehicle.damping }
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

	std::optional<TruckDefinition> TruckDefinition::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != TRUCK_FORMAT_VERSION)
			return std::nullopt;

		const nlohmann::json& body       = json.at("body");
		const nlohmann::json& wheels     = json.at("wheels");
		const nlohmann::json& suspension = json.at("suspension");
		const nlohmann::json& engine     = json.at("engine");
		const nlohmann::json& steering   = json.at("steering");
		const nlohmann::json& tyres      = json.at("tyres");

		TruckDefinition definition =
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
				.rollInfluence      = tyres.at("rollInfluence").get<float>()
			}
		};

		for (const nlohmann::json& wheel : wheels.at("mounts"))
			definition.vehicle.wheels.push_back(DeserializeWheel(wheel));

		for (const nlohmann::json& part : json.at("parts"))
			definition.parts.push_back(DeserializePart(part));

		if (!IsDrivable(definition))
		{
			Fs::ReportMalformedJson("a truck needs a positive mass and at least one driven wheel");
			return std::nullopt;
		}

		return definition;
	}

	lunar::Physics::RigidBody CreateTruckBody(lunar::Scene&          scene,
	                                          const TruckDefinition& definition,
	                                          const glm::vec3&       position,
	                                          const glm::quat&       rotation)
	{
		lunar::Physics::RigidBody body(scene, position, rotation, lunar::Physics::BodyType::eDynamic);
		body.addBox(definition.colliderHalfExtents, definition.colliderOffset, lunar::Physics::VEHICLE_CATEGORY);
		body.setMass(definition.mass, definition.centerOfMass);
		body.getBody().setIsAllowedToSleep(false);
		return body;
	}

	Truck::Truck(lunar::Scene&                scene,
	             lunar::Render::MeshRegistry& meshes,
	             const TruckDefinition&       definition,
	             const glm::vec3&             position,
	             const glm::quat&             rotation) noexcept
		: meshes(meshes),
		chassis(scene.createGameObject(definition.name)),
		vehicle(definition.vehicle),
		wheelWidth(definition.wheelWidth)
	{
		chassis->getTransform().position = position;
		chassis->getTransform().rotation = rotation;
		chassis->addComponent<lunar::Physics::RigidBody>(CreateTruckBody(scene, definition, position, rotation));

		for (const TruckPart& part : definition.parts)
			createBox(part.color, part.halfExtents, part.offset);

		for (const lunar::Physics::WheelSettings& wheel : definition.vehicle.wheels)
			wheels.push_back(createBox(definition.wheelColor, glm::vec3(wheelWidth * HALF, wheel.radius, wheel.radius), wheel.mountPoint));

		updateWheels();
	}

	Truck::~Truck() noexcept
	{
		chassis->destroy();
		for (const lunar::Render::MeshHandle mesh : ownedMeshes)
			meshes.destroy(mesh);
	}

	void Truck::drive(const lunar::Physics::VehicleInput& input, float delta_time)
	{
		lunar::Physics::RigidBody& body = *chassis->getComponent<lunar::Physics::RigidBody>();
		if (body.getBody().isActive())
			vehicle.update(body, input, delta_time);
	}

	void Truck::setSimulated(bool active)
	{
		rp3d::RigidBody& body = chassis->getComponent<lunar::Physics::RigidBody>()->getBody();
		if (body.isActive() != active)
			body.setIsActive(active);

		simulated = active;
	}

	void Truck::updateWheels()
	{
		for (size_t wheel = 0; wheel < wheels.size(); wheel++)
		{
			lunar::Transform& transform = wheels[wheel]->getTransform();
			transform.position = vehicle.getWheelLocalPosition(wheel);
			transform.rotation = vehicle.getWheelLocalRotation(wheel);
		}
	}

	const lunar::Transform& Truck::getTransform() const
	{
		return chassis->getTransform();
	}

	const lunar::Physics::RaycastVehicle& Truck::getVehicle() const
	{
		return vehicle;
	}

	lunar::Physics::RaycastVehicle& Truck::editVehicle()
	{
		return vehicle;
	}

	bool Truck::isSimulated() const
	{
		return simulated;
	}

	lunar::GameObject Truck::createBox(const glm::vec3& color, const glm::vec3& half_extents, const glm::vec3& offset)
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
