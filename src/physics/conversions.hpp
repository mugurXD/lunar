#pragma once
#include <lunar/physics/physics_world.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace lunar::Physics
{
	inline JPH::Vec3 ToPhysics(const glm::vec3& vector)
	{
		return JPH::Vec3(vector.x, vector.y, vector.z);
	}

	inline JPH::Quat ToPhysics(const glm::quat& rotation)
	{
		return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w);
	}

	inline JPH::BodyID ToPhysics(BodyId id)
	{
		return JPH::BodyID(static_cast<uint32_t>(id));
	}

	inline glm::vec3 ToGlm(JPH::Vec3Arg vector)
	{
		return glm::vec3(vector.GetX(), vector.GetY(), vector.GetZ());
	}

	inline glm::quat ToGlm(JPH::QuatArg rotation)
	{
		return glm::quat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
	}

	inline BodyId ToBodyId(const JPH::BodyID& id)
	{
		return static_cast<BodyId>(id.GetIndexAndSequenceNumber());
	}
}
