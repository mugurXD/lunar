#pragma once
#include <lunar/api.hpp>
#include <lunar/physics/raycast.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace lunar::Physics
{
	enum class LUNAR_API BodyId : uint32_t {};

	constexpr BodyId INVALID_BODY_ID = static_cast<BodyId>(std::numeric_limits<uint32_t>::max());

	struct PhysicsBackend;

	class LUNAR_API PhysicsWorld
	{
	public:
		PhysicsWorld()  noexcept;
		~PhysicsWorld() noexcept;

		PhysicsWorld(const PhysicsWorld&)            = delete;
		PhysicsWorld& operator=(const PhysicsWorld&) = delete;

		void                      step(float delta_time);
		void                      shift(const glm::vec3& offset);
		std::optional<RaycastHit> castRay(const glm::vec3& from, const glm::vec3& to, uint16_t category_mask) const;

	private:
		std::unique_ptr<PhysicsBackend> backend;

		friend class RigidBody;
	};
}
