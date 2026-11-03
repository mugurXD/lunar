#include <lunar/physics/rigid_body.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/debug.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <utility>
#include <variant>

namespace lunar::Physics
{
	namespace
	{
		constexpr uint32_t TRIANGLE_INDICES = 3;
		constexpr uint32_t DEFAULT_MATERIAL = 0;
		constexpr float    BODY_FRICTION    = 0.3f;
		constexpr float    BODY_RESTITUTION = 0.5f;
		constexpr float    BODY_DAMPING     = 0.f;

		struct ShapePart
		{
			JPH::Ref<JPH::ShapeSettings> settings = nullptr;
			glm::vec3                    offset   = {};
		};

		JPH::EMotionType ToPhysics(BodyType type)
		{
			switch (type)
			{
				case BodyType::eStatic:    return JPH::EMotionType::Static;
				case BodyType::eKinematic: return JPH::EMotionType::Kinematic;
				default:                   return JPH::EMotionType::Dynamic;
			}
		}

		ShapePart MakeShape(const BoxDesc& desc)
		{
			return { .settings = new JPH::BoxShapeSettings(Physics::ToPhysics(desc.halfExtents)), .offset = desc.offset };
		}

		ShapePart MakeShape(const HeightfieldDesc& desc)
		{
			return { .settings = new JPH::HeightFieldShapeSettings(desc.heights.data(), JPH::Vec3::sZero(), JPH::Vec3(desc.spacing, 1.f, desc.spacing), desc.samplesPerSide) };
		}

		ShapePart MakeShape(const TriangleMeshDesc& desc)
		{
			JPH::VertexList vertices;
			vertices.reserve(desc.vertices.size());
			for (const glm::vec3& vertex : desc.vertices)
				vertices.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));

			JPH::IndexedTriangleList triangles;
			triangles.reserve(desc.indices.size() / TRIANGLE_INDICES);
			for (size_t index = 0; index + TRIANGLE_INDICES <= desc.indices.size(); index += TRIANGLE_INDICES)
				triangles.push_back(JPH::IndexedTriangle(desc.indices[index], desc.indices[index + 1], desc.indices[index + 2], DEFAULT_MATERIAL));

			return { .settings = new JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)) };
		}

		JPH::ShapeSettings::ShapeResult CreateShape(std::span<const ShapeDesc> shapes)
		{
			JPH::StaticCompoundShapeSettings compound;
			for (const ShapeDesc& shape : shapes)
			{
				const ShapePart part = std::visit([](const auto& geometry) { return MakeShape(geometry); }, shape.geometry);
				part.settings->mUserData = shape.category;

				const JPH::ShapeSettings::ShapeResult result = part.settings->Create();
				if (result.HasError())
					DEBUG_ERROR("Failed to create a collider: {}", result.GetError().c_str());
				else
					compound.AddShape(Physics::ToPhysics(part.offset), JPH::Quat::sIdentity(), result.Get());
			}

			if (compound.mSubShapes.size() != 1)
				return compound.Create();

			const JPH::CompoundShapeSettings::SubShapeSettings& part = compound.mSubShapes.front();
			return JPH::RotatedTranslatedShapeSettings(part.mPosition, part.mRotation, part.mShapePtr).Create();
		}

		JPH::ShapeSettings::ShapeResult OffsetCenterOfMass(const JPH::ShapeSettings::ShapeResult& shape, const glm::vec3& center_of_mass)
		{
			return JPH::OffsetCenterOfMassShapeSettings(Physics::ToPhysics(center_of_mass) - shape.Get()->GetCenterOfMass(), shape.Get()).Create();
		}

		BodyId CreateBody(PhysicsBackend& backend, const BodyDesc& desc)
		{
			JPH::ShapeSettings::ShapeResult shape = CreateShape(desc.shapes);
			if (shape.IsValid() && desc.massOverride.has_value())
				shape = OffsetCenterOfMass(shape, desc.massOverride->centerOfMass);

			if (shape.HasError())
			{
				DEBUG_ERROR("Failed to create a rigid body: {}", shape.GetError().c_str());
				return INVALID_BODY_ID;
			}

			JPH::BodyCreationSettings settings(shape.Get(), Physics::ToPhysics(desc.position), Physics::ToPhysics(desc.rotation), ToPhysics(desc.type), desc.type == BodyType::eStatic ? STATIC_LAYER : MOVING_LAYER);
			settings.mAllowSleeping  = desc.sleepAllowed;
			settings.mFriction       = BODY_FRICTION;
			settings.mRestitution    = BODY_RESTITUTION;
			settings.mLinearDamping  = BODY_DAMPING;
			settings.mAngularDamping = BODY_DAMPING;

			if (desc.massOverride.has_value())
			{
				settings.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
				settings.mMassPropertiesOverride.mMass = desc.massOverride->mass;
			}

			const JPH::EActivation activation = desc.type == BodyType::eStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate;
			const JPH::BodyID      body       = backend.bodies().CreateAndAddBody(settings, activation);
			if (body.IsInvalid())
				DEBUG_ERROR("The physics world has run out of bodies");

			return ToBodyId(body);
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
		world->backend->bodies().AddForce(Physics::ToPhysics(id), Physics::ToPhysics(force));
	}

	void RigidBody::applyForce(const glm::vec3& force, const glm::vec3& point)
	{
		world->backend->bodies().AddForce(Physics::ToPhysics(id), Physics::ToPhysics(force), Physics::ToPhysics(point));
	}

	void RigidBody::applyAngularAcceleration(const glm::vec3& acceleration)
	{
		JPH::BodyInterface& bodies = world->backend->bodies();
		const JPH::BodyID   body   = Physics::ToPhysics(id);
		bodies.AddTorque(body, bodies.GetInverseInertia(body).Inversed3x3().Multiply3x3(Physics::ToPhysics(acceleration)));
	}

	void RigidBody::setTransform(const glm::vec3& position, const glm::quat& rotation)
	{
		world->backend->bodies().SetPositionAndRotation(Physics::ToPhysics(id), Physics::ToPhysics(position), Physics::ToPhysics(rotation), JPH::EActivation::Activate);
	}

	void RigidBody::setLinearVelocity(const glm::vec3& velocity)
	{
		world->backend->bodies().SetLinearVelocity(Physics::ToPhysics(id), Physics::ToPhysics(velocity));
	}

	void RigidBody::setAngularVelocity(const glm::vec3& velocity)
	{
		world->backend->bodies().SetAngularVelocity(Physics::ToPhysics(id), Physics::ToPhysics(velocity));
	}

	void RigidBody::setActive(bool active)
	{
		JPH::BodyInterface& bodies = world->backend->bodies();
		const JPH::BodyID   body   = Physics::ToPhysics(id);
		if (bodies.IsAdded(body) == active)
			return;

		if (active)
			bodies.AddBody(body, JPH::EActivation::Activate);
		else
			bodies.RemoveBody(body);
	}

	glm::vec3 RigidBody::getPosition() const
	{
		return ToGlm(world->backend->bodies().GetPosition(Physics::ToPhysics(id)));
	}

	glm::quat RigidBody::getRotation() const
	{
		return ToGlm(world->backend->bodies().GetRotation(Physics::ToPhysics(id)));
	}

	glm::vec3 RigidBody::getCenterOfMass() const
	{
		return ToGlm(world->backend->bodies().GetCenterOfMassPosition(Physics::ToPhysics(id)));
	}

	glm::vec3 RigidBody::getLinearVelocity() const
	{
		return ToGlm(world->backend->bodies().GetLinearVelocity(Physics::ToPhysics(id)));
	}

	glm::vec3 RigidBody::getAngularVelocity() const
	{
		return ToGlm(world->backend->bodies().GetAngularVelocity(Physics::ToPhysics(id)));
	}

	float RigidBody::getMass() const
	{
		const JPH::BodyLockRead lock(world->backend->system.GetBodyLockInterface(), Physics::ToPhysics(id));
		if (!lock.Succeeded() || !lock.GetBody().IsDynamic())
			return 0.f;

		return 1.f / lock.GetBody().GetMotionProperties()->GetInverseMass();
	}

	bool RigidBody::isActive() const
	{
		return world->backend->bodies().IsAdded(Physics::ToPhysics(id));
	}

	const PhysicsWorld& RigidBody::getWorld() const
	{
		return *world;
	}

	void RigidBody::capturePose()
	{
		JPH::RVec3 position;
		JPH::Quat  rotation;
		world->backend->bodies().GetPositionAndRotation(Physics::ToPhysics(id), position, rotation);

		previousPosition = currentPosition;
		previousRotation = currentRotation;
		currentPosition  = ToGlm(position);
		currentRotation  = ToGlm(rotation);
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

		JPH::BodyInterface& bodies = world->backend->bodies();
		const JPH::BodyID   body   = Physics::ToPhysics(std::exchange(id, INVALID_BODY_ID));
		if (bodies.IsAdded(body))
			bodies.RemoveBody(body);

		bodies.DestroyBody(body);
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
