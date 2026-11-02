#include <lunar/physics/raycast.hpp>
#include <lunar/core/scene.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

namespace lunar::Physics
{
	namespace
	{
		class ClosestHit final : public rp3d::RaycastCallback
		{
		public:
			rp3d::decimal notifyRaycastHit(const rp3d::RaycastInfo& info) override
			{
				hit = RaycastHit { ToGlm(info.worldPoint), ToGlm(info.worldNormal), info.hitFraction, static_cast<uint16_t>(info.collider->getCollisionCategoryBits()) };
				return info.hitFraction;
			}

			std::optional<RaycastHit> hit = std::nullopt;
		};
	}

	std::optional<RaycastHit> PhysicsWorld::castRay(const glm::vec3& from, const glm::vec3& to, uint16_t category_mask) const
	{
		ClosestHit callback;
		backend->world->raycast(rp3d::Ray(ToPhysics(from), ToPhysics(to)), &callback, category_mask);
		return callback.hit;
	}

	std::optional<RaycastHit> CastRay(Scene& scene, const glm::vec3& from, const glm::vec3& to, uint16_t category_mask)
	{
		return scene.getPhysicsWorld().castRay(from, to, category_mask);
	}
}
