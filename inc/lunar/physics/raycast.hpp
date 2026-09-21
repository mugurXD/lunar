#pragma once
#include <lunar/api.hpp>

#include <glm/glm.hpp>
#include <lunar/physics/reactphysics.hpp>

#include <cstdint>
#include <optional>

namespace lunar::Physics
{
	struct LUNAR_API RaycastHit
	{
		glm::vec3 point    = {};
		glm::vec3 normal   = {};
		float     fraction = 0.f;
		uint16_t  category = 0;
	};

	LUNAR_API std::optional<RaycastHit> CastRay(const rp3d::PhysicsWorld& world, const glm::vec3& from, const glm::vec3& to, uint16_t category_mask);
}
