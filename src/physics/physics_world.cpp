#include <lunar/physics/physics_world.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

namespace lunar::Physics
{
	namespace
	{
		rp3d::PhysicsCommon& SharedCommon()
		{
			static rp3d::PhysicsCommon common;
			return common;
		}
	}

	PhysicsWorld::PhysicsWorld() noexcept
		: backend(std::make_unique<PhysicsBackend>(PhysicsBackend { .common = SharedCommon(), .world = SharedCommon().createPhysicsWorld() }))
	{
	}

	PhysicsWorld::~PhysicsWorld() noexcept
	{
		backend->common.destroyPhysicsWorld(backend->world);
	}

	void PhysicsWorld::step(float delta_time)
	{
		backend->world->update(delta_time);
	}

	void PhysicsWorld::shift(const glm::vec3& offset)
	{
		for (uint32_t index = 0; index < backend->world->getNbRigidBodies(); index++)
		{
			rp3d::RigidBody&       body      = *backend->world->getRigidBody(index);
			const rp3d::Vector3    velocity  = body.getLinearVelocity();
			const rp3d::Transform& transform = body.getTransform();

			// rp3d adds spin × displacement to the velocity on setTransform, which would fling every rotating body across a multi-kilometre shift.
			body.setTransform(rp3d::Transform(transform.getPosition() + ToPhysics(offset), transform.getOrientation()));
			body.setLinearVelocity(velocity);
		}
	}
}
