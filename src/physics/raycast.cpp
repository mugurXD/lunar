#include <lunar/physics/raycast.hpp>
#include <lunar/core/scene.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>

namespace lunar::Physics
{
	namespace
	{
		class ClosestHit final : public JPH::CastRayCollector
		{
		public:
			ClosestHit(const JPH::RRayCast& ray, uint16_t category_mask) noexcept
				: ray(ray),
				categoryMask(category_mask)
			{
			}

			void AddHit(const JPH::RayCastResult& result) override
			{
				const JPH::TransformedShape& shape    = *GetContext();
				const uint16_t               category = static_cast<uint16_t>(shape.GetSubShapeUserData(result.mSubShapeID2));
				if ((category & categoryMask) == 0 || result.mFraction >= GetEarlyOutFraction())
					return;

				const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
				hit = RaycastHit { ToGlm(point), ToGlm(shape.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point)), result.mFraction, category };
				UpdateEarlyOutFraction(result.mFraction);
			}

			std::optional<RaycastHit> hit = std::nullopt;

		private:
			JPH::RRayCast ray          = {};
			uint16_t      categoryMask = 0;
		};
	}

	std::optional<RaycastHit> PhysicsWorld::castRay(const glm::vec3& from, const glm::vec3& to, uint16_t category_mask) const
	{
		const JPH::RRayCast ray(ToPhysics(from), ToPhysics(to - from));
		ClosestHit          collector(ray, category_mask);
		backend->system.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector);
		return collector.hit;
	}

	std::optional<RaycastHit> CastRay(Scene& scene, const glm::vec3& from, const glm::vec3& to, uint16_t category_mask)
	{
		return scene.getPhysicsWorld().castRay(from, to, category_mask);
	}
}
