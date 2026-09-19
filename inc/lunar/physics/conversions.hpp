#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <lunar/physics/reactphysics.hpp>

namespace lunar::Physics
{
	inline rp3d::Vector3 ToPhysics(const glm::vec3& vector)
	{
		return rp3d::Vector3(vector.x, vector.y, vector.z);
	}

	inline rp3d::Quaternion ToPhysics(const glm::quat& rotation)
	{
		return rp3d::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
	}

	inline glm::vec3 ToGlm(const rp3d::Vector3& vector)
	{
		return glm::vec3(vector.x, vector.y, vector.z);
	}

	inline glm::quat ToGlm(const rp3d::Quaternion& rotation)
	{
		return glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
	}
}
