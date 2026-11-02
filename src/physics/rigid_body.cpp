#include <lunar/physics/rigid_body.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

#include <utility>
#include <variant>

namespace lunar::Physics
{
	namespace
	{
		constexpr uint32_t TRIANGLE_INDICES = 3;

		rp3d::BodyType ToPhysics(BodyType type)
		{
			switch (type)
			{
				case BodyType::eStatic:    return rp3d::BodyType::STATIC;
				case BodyType::eKinematic: return rp3d::BodyType::KINEMATIC;
				default:                   return rp3d::BodyType::DYNAMIC;
			}
		}

		rp3d::Collider* AddShape(rp3d::PhysicsCommon& common, BodyRecord& record, const BoxDesc& desc)
		{
			rp3d::BoxShape* shape = common.createBoxShape(Physics::ToPhysics(desc.halfExtents));
			record.shapes.push_back(shape);
			return record.body->addCollider(shape, rp3d::Transform(Physics::ToPhysics(desc.offset), rp3d::Quaternion::identity()));
		}

		rp3d::Collider* AddShape(rp3d::PhysicsCommon& common, BodyRecord& record, const HeightfieldDesc& desc)
		{
			const int                  samples  = static_cast<int>(desc.samplesPerSide);
			std::vector<rp3d::Message> messages = {};
			rp3d::HeightField*         field    = common.createHeightField(samples, samples, desc.heights.data(), rp3d::HeightField::HeightDataType::HEIGHT_FLOAT_TYPE, messages);
			if (field == nullptr)
			{
				DEBUG_ERROR("Failed to create a heightfield collider");
				return nullptr;
			}

			const float             half_extent = static_cast<float>(desc.samplesPerSide - 1) * desc.spacing * 0.5f;
			const float             middle      = (field->getMinHeight() + field->getMaxHeight()) * 0.5f;
			rp3d::HeightFieldShape* shape       = common.createHeightFieldShape(field, rp3d::Vector3(desc.spacing, 1.f, desc.spacing));

			record.shapes.push_back(shape);
			return record.body->addCollider(shape, rp3d::Transform(rp3d::Vector3(half_extent, middle, half_extent), rp3d::Quaternion::identity()));
		}

		rp3d::Collider* AddShape(rp3d::PhysicsCommon& common, BodyRecord& record, const TriangleMeshDesc& desc)
		{
			const rp3d::TriangleVertexArray triangles(static_cast<uint32_t>(desc.vertices.size()), desc.vertices.data(), sizeof(glm::vec3),
			                                          static_cast<uint32_t>(desc.indices.size() / TRIANGLE_INDICES), desc.indices.data(), sizeof(uint32_t) * TRIANGLE_INDICES,
			                                          rp3d::TriangleVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
			                                          rp3d::TriangleVertexArray::IndexDataType::INDEX_INTEGER_TYPE);

			std::vector<rp3d::Message> messages = {};
			rp3d::TriangleMesh*        mesh     = common.createTriangleMesh(triangles, messages);
			if (mesh == nullptr)
			{
				DEBUG_ERROR("Failed to create a triangle mesh collider");
				return nullptr;
			}

			rp3d::ConcaveMeshShape* shape = common.createConcaveMeshShape(mesh);
			record.shapes.push_back(shape);
			record.triangleMeshes.push_back(mesh);
			return record.body->addCollider(shape, rp3d::Transform::identity());
		}

		void ApplyMass(rp3d::RigidBody& body, const MassDesc& desc)
		{
			float volume = 0.f;
			for (uint32_t index = 0; index < body.getNbColliders(); index++)
				volume += body.getCollider(index)->getCollisionShape()->getVolume();

			for (uint32_t index = 0; index < body.getNbColliders(); index++)
				body.getCollider(index)->getMaterial().setMassDensity(desc.mass / volume);

			body.updateMassPropertiesFromColliders();
			body.setLocalCenterOfMass(Physics::ToPhysics(desc.centerOfMass));
		}

		BodyId CreateBody(PhysicsBackend& backend, const BodyDesc& desc)
		{
			rp3d::RigidBody* body   = backend.world->createRigidBody(rp3d::Transform(Physics::ToPhysics(desc.position), Physics::ToPhysics(desc.rotation)));
			const BodyId     id     = static_cast<BodyId>(body->getEntity().id);
			BodyRecord&      record = backend.bodies.emplace(id, BodyRecord { .body = body }).first->second;

			body->setType(ToPhysics(desc.type));
			for (const ShapeDesc& shape : desc.shapes)
			{
				rp3d::Collider* collider = std::visit([&](const auto& geometry) { return AddShape(backend.common, record, geometry); }, shape.geometry);
				if (collider != nullptr)
					collider->setCollisionCategoryBits(shape.category);
			}

			if (desc.massOverride.has_value())
				ApplyMass(*body, *desc.massOverride);

			body->setIsAllowedToSleep(desc.sleepAllowed);
			return id;
		}

		void DestroyShape(rp3d::PhysicsCommon& common, rp3d::CollisionShape* shape)
		{
			const rp3d::CollisionShapeName name = shape->getName();
			if (name == rp3d::CollisionShapeName::BOX)
				common.destroyBoxShape(static_cast<rp3d::BoxShape*>(shape));

			if (name == rp3d::CollisionShapeName::TRIANGLE_MESH)
				common.destroyConcaveMeshShape(static_cast<rp3d::ConcaveMeshShape*>(shape));

			if (name != rp3d::CollisionShapeName::HEIGHTFIELD)
				return;

			rp3d::HeightFieldShape* heightfield = static_cast<rp3d::HeightFieldShape*>(shape);
			rp3d::HeightField*      field       = heightfield->getHeightField();
			common.destroyHeightFieldShape(heightfield);
			common.destroyHeightField(field);
		}
	}

	RigidBody::RigidBody(Scene& scene, const BodyDesc& desc) noexcept
		: world(&scene.getPhysicsWorld()),
		id(CreateBody(*world->backend, desc)),
		previousPosition(desc.position),
		currentPosition(desc.position),
		previousRotation(desc.rotation),
		currentRotation(desc.rotation)
	{
	}

	RigidBody::~RigidBody() noexcept
	{
		release();
	}

	RigidBody::RigidBody(RigidBody&& other) noexcept
		: world(other.world),
		id(std::exchange(other.id, INVALID_BODY_ID)),
		previousPosition(other.previousPosition),
		currentPosition(other.currentPosition),
		previousRotation(other.previousRotation),
		currentRotation(other.currentRotation)
	{
	}

	RigidBody& RigidBody::operator=(RigidBody&& other) noexcept
	{
		if (this == &other)
			return *this;

		release();
		world            = other.world;
		id               = std::exchange(other.id, INVALID_BODY_ID);
		previousPosition = other.previousPosition;
		currentPosition  = other.currentPosition;
		previousRotation = other.previousRotation;
		currentRotation  = other.currentRotation;
		return *this;
	}

	void RigidBody::applyForce(const glm::vec3& force)
	{
		world->backend->body(id).applyWorldForceAtCenterOfMass(Physics::ToPhysics(force));
	}

	void RigidBody::applyForce(const glm::vec3& force, const glm::vec3& point)
	{
		world->backend->body(id).applyWorldForceAtWorldPosition(Physics::ToPhysics(force), Physics::ToPhysics(point));
	}

	void RigidBody::applyAngularAcceleration(const glm::vec3& acceleration)
	{
		rp3d::RigidBody& body               = world->backend->body(id);
		const glm::vec3  local_acceleration = glm::inverse(ToGlm(body.getTransform().getOrientation())) * acceleration;
		body.applyLocalTorque(Physics::ToPhysics(local_acceleration * ToGlm(body.getLocalInertiaTensor())));
	}

	void RigidBody::setTransform(const glm::vec3& position, const glm::quat& rotation)
	{
		world->backend->body(id).setTransform(rp3d::Transform(Physics::ToPhysics(position), Physics::ToPhysics(rotation)));
	}

	void RigidBody::setLinearVelocity(const glm::vec3& velocity)
	{
		world->backend->body(id).setLinearVelocity(Physics::ToPhysics(velocity));
	}

	void RigidBody::setAngularVelocity(const glm::vec3& velocity)
	{
		world->backend->body(id).setAngularVelocity(Physics::ToPhysics(velocity));
	}

	void RigidBody::setActive(bool active)
	{
		world->backend->body(id).setIsActive(active);
	}

	glm::vec3 RigidBody::getPosition() const
	{
		return ToGlm(world->backend->body(id).getTransform().getPosition());
	}

	glm::quat RigidBody::getRotation() const
	{
		return ToGlm(world->backend->body(id).getTransform().getOrientation());
	}

	glm::vec3 RigidBody::getCenterOfMass() const
	{
		const rp3d::RigidBody& body = world->backend->body(id);
		return ToGlm(body.getWorldPoint(body.getLocalCenterOfMass()));
	}

	glm::vec3 RigidBody::getLinearVelocity() const
	{
		return ToGlm(world->backend->body(id).getLinearVelocity());
	}

	glm::vec3 RigidBody::getAngularVelocity() const
	{
		return ToGlm(world->backend->body(id).getAngularVelocity());
	}

	float RigidBody::getMass() const
	{
		return world->backend->body(id).getMass();
	}

	bool RigidBody::isActive() const
	{
		return world->backend->body(id).isActive();
	}

	const PhysicsWorld& RigidBody::getWorld() const
	{
		return *world;
	}

	void RigidBody::capturePose()
	{
		const rp3d::Transform& transform = world->backend->body(id).getTransform();

		previousPosition = currentPosition;
		previousRotation = currentRotation;
		currentPosition  = ToGlm(transform.getPosition());
		currentRotation  = ToGlm(transform.getOrientation());
	}

	void RigidBody::applyPose(Transform& transform, float alpha) const
	{
		transform.position = glm::mix(previousPosition, currentPosition, alpha);
		transform.rotation = glm::slerp(previousRotation, currentRotation, alpha);
	}

	void RigidBody::shiftPoses(const glm::vec3& offset)
	{
		previousPosition += offset;
		currentPosition  += offset;
	}

	void RigidBody::release()
	{
		if (id == INVALID_BODY_ID)
			return;

		PhysicsBackend&   backend = *world->backend;
		const auto        node    = backend.bodies.extract(std::exchange(id, INVALID_BODY_ID));
		const BodyRecord& record  = node.mapped();

		backend.world->destroyRigidBody(record.body);
		for (rp3d::CollisionShape* shape : record.shapes)
			DestroyShape(backend.common, shape);

		for (rp3d::TriangleMesh* mesh : record.triangleMeshes)
			backend.common.destroyTriangleMesh(mesh);
	}

	void CapturePhysicsPoses(Scene& scene, const FrameTime&)
	{
		scene.forEach<RigidBody>([](Entity, RigidBody& body) {
			body.capturePose();
		});
	}

	void ApplyPhysicsPoses(Scene& scene, const FrameTime& frame_time)
	{
		scene.forEach<RigidBody, Transform>([&frame_time](Entity, const RigidBody& body, Transform& transform) {
			body.applyPose(transform, frame_time.fixedAlpha);
		});
	}

	void ShiftPhysics(Scene& scene, const glm::vec3& offset)
	{
		scene.getPhysicsWorld().shift(offset);
		scene.forEach<RigidBody>([&offset](Entity, RigidBody& body) {
			body.shiftPoses(offset);
		});
	}
}
