#include <lunar/physics/rigid_body.hpp>
#include <lunar/physics/conversions.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug.hpp>

#include <utility>

namespace lunar::Physics
{
	namespace
	{
		rp3d::BodyType ToPhysics(BodyType type)
		{
			switch (type)
			{
				case BodyType::eStatic:    return rp3d::BodyType::STATIC;
				case BodyType::eKinematic: return rp3d::BodyType::KINEMATIC;
				default:                   return rp3d::BodyType::DYNAMIC;
			}
		}
	}

	RigidBody::RigidBody(Scene& scene, const glm::vec3& position, const glm::quat& rotation, BodyType type) noexcept
		: common(&scene.getPhysicsCommon()),
		world(scene.getPhysicsWorld()),
		body(world->createRigidBody(rp3d::Transform(Physics::ToPhysics(position), Physics::ToPhysics(rotation)))),
		previousPosition(position),
		currentPosition(position),
		previousRotation(rotation),
		currentRotation(rotation)
	{
		body->setType(ToPhysics(type));
	}

	RigidBody::~RigidBody() noexcept
	{
		release();
	}

	RigidBody::RigidBody(RigidBody&& other) noexcept
		: common(other.common),
		world(other.world),
		body(std::exchange(other.body, nullptr)),
		shapes(std::move(other.shapes)),
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
		common           = other.common;
		world            = other.world;
		body             = std::exchange(other.body, nullptr);
		shapes           = std::move(other.shapes);
		previousPosition = other.previousPosition;
		currentPosition  = other.currentPosition;
		previousRotation = other.previousRotation;
		currentRotation  = other.currentRotation;
		return *this;
	}

	rp3d::Collider* RigidBody::addBox(const glm::vec3& half_extents, const glm::vec3& offset, uint16_t category)
	{
		rp3d::BoxShape* shape    = common->createBoxShape(Physics::ToPhysics(half_extents));
		rp3d::Collider* collider = body->addCollider(shape, rp3d::Transform(Physics::ToPhysics(offset), rp3d::Quaternion::identity()));

		collider->setCollisionCategoryBits(category);
		shapes.push_back(shape);
		return collider;
	}

	rp3d::Collider* RigidBody::addHeightfield(const HeightfieldDesc& desc, uint16_t category)
	{
		const int                  samples  = static_cast<int>(desc.samplesPerSide);
		std::vector<rp3d::Message> messages = {};
		rp3d::HeightField*         field    = common->createHeightField(samples, samples, desc.heights.data(), rp3d::HeightField::HeightDataType::HEIGHT_FLOAT_TYPE, messages);
		if (field == nullptr)
		{
			DEBUG_ERROR("Failed to create a heightfield collider");
			return nullptr;
		}

		const float             half_extent = static_cast<float>(desc.samplesPerSide - 1) * desc.spacing * 0.5f;
		const float             middle      = (field->getMinHeight() + field->getMaxHeight()) * 0.5f;
		rp3d::HeightFieldShape* shape       = common->createHeightFieldShape(field, rp3d::Vector3(desc.spacing, 1.f, desc.spacing));
		rp3d::Collider*         collider    = body->addCollider(shape, rp3d::Transform(rp3d::Vector3(half_extent, middle, half_extent), rp3d::Quaternion::identity()));

		collider->setCollisionCategoryBits(category);
		shapes.push_back(shape);
		return collider;
	}

	void RigidBody::setMass(float mass, const glm::vec3& center_of_mass)
	{
		float volume = 0.f;
		for (uint32_t index = 0; index < body->getNbColliders(); index++)
			volume += body->getCollider(index)->getCollisionShape()->getVolume();

		for (uint32_t index = 0; index < body->getNbColliders(); index++)
			body->getCollider(index)->getMaterial().setMassDensity(mass / volume);

		body->updateMassPropertiesFromColliders();
		body->setLocalCenterOfMass(Physics::ToPhysics(center_of_mass));
	}

	rp3d::RigidBody& RigidBody::getBody()
	{
		return *body;
	}

	rp3d::PhysicsWorld& RigidBody::getWorld()
	{
		return *world;
	}

	void RigidBody::capturePose()
	{
		const rp3d::Transform& transform = body->getTransform();

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

	void RigidBody::release()
	{
		if (body == nullptr)
			return;

		world->destroyRigidBody(body);
		body = nullptr;

		for (rp3d::CollisionShape* shape : shapes)
		{
			if (shape->getName() == rp3d::CollisionShapeName::BOX)
				common->destroyBoxShape(static_cast<rp3d::BoxShape*>(shape));

			if (shape->getName() != rp3d::CollisionShapeName::HEIGHTFIELD)
				continue;

			rp3d::HeightFieldShape* heightfield = static_cast<rp3d::HeightFieldShape*>(shape);
			rp3d::HeightField*      field       = heightfield->getHeightField();
			common->destroyHeightFieldShape(heightfield);
			common->destroyHeightField(field);
		}

		shapes.clear();
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
}
