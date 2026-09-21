#include <lunar/physics/raycast.hpp>
#include <lunar/physics/conversions.hpp>

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

	std::optional<RaycastHit> CastRay(const rp3d::PhysicsWorld& world, const glm::vec3& from, const glm::vec3& to, uint16_t category_mask)
	{
		ClosestHit callback;
		world.raycast(rp3d::Ray(ToPhysics(from), ToPhysics(to)), &callback, category_mask);
		return callback.hit;
	}
}
