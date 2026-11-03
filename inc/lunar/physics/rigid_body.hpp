#pragma once
#include <lunar/api.hpp>
#include <lunar/core/time.hpp>

#include <lunar/physics/physics_world.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace lunar
{
	class Scene;
	struct Transform;
}

namespace lunar::Physics
{
	constexpr uint16_t TERRAIN_CATEGORY = 1 << 0;
	constexpr uint16_t VEHICLE_CATEGORY = 1 << 1;
	constexpr uint16_t ROAD_CATEGORY    = 1 << 2;
	constexpr uint16_t PROP_CATEGORY    = 1 << 3;

	enum class LUNAR_API BodyType
	{
		eStatic,
		eKinematic,
		eDynamic
	};

	struct LUNAR_API HeightfieldDesc
	{
		std::span<const float> heights        = {};
		uint32_t               samplesPerSide = 0;
		float                  spacing        = 1.f;
	};

	struct LUNAR_API TriangleMeshDesc
	{
		std::span<const glm::vec3> vertices = {};
		std::span<const uint32_t>  indices  = {};
	};

	struct LUNAR_API BoxDesc
	{
		glm::vec3 halfExtents = {};
		glm::vec3 offset      = {};
	};

	struct LUNAR_API ConvexHullDesc
	{
		std::span<const glm::vec3> points = {};
	};

	struct LUNAR_API ShapeDesc
	{
		std::variant<BoxDesc, HeightfieldDesc, TriangleMeshDesc, ConvexHullDesc> geometry = {};
		uint16_t                                                                 category = 0;
	};

	struct LUNAR_API MassDesc
	{
		float     mass         = 0.f;
		glm::vec3 centerOfMass = {};
	};

	struct LUNAR_API BodyDesc
	{
		glm::vec3                  position     = {};
		glm::quat                  rotation     = glm::quat(1.f, 0.f, 0.f, 0.f);
		BodyType                   type         = BodyType::eStatic;
		std::span<const ShapeDesc> shapes       = {};
		std::optional<MassDesc>    massOverride = std::nullopt;
		bool                       sleepAllowed = true;
		bool                       startAsleep  = false;
	};

	class LUNAR_API RigidBody
	{
	public:
		RigidBody(Scene& scene, const BodyDesc& desc) noexcept;
		~RigidBody() noexcept;

		RigidBody(RigidBody&& other) noexcept;
		RigidBody& operator=(RigidBody&& other) noexcept;

		RigidBody(const RigidBody&)            = delete;
		RigidBody& operator=(const RigidBody&) = delete;

		void applyForce(const glm::vec3& force);
		void applyForce(const glm::vec3& force, const glm::vec3& point);
		void applyAngularAcceleration(const glm::vec3& acceleration);

		void setTransform(const glm::vec3& position, const glm::quat& rotation);
		void setLinearVelocity(const glm::vec3& velocity);
		void setAngularVelocity(const glm::vec3& velocity);
		void setActive(bool active);

		glm::vec3           getPosition()        const;
		glm::quat           getRotation()        const;
		glm::vec3           getCenterOfMass()    const;
		glm::vec3           getLinearVelocity()  const;
		glm::vec3           getAngularVelocity() const;
		float               getMass()            const;
		bool                isActive()           const;
		const PhysicsWorld& getWorld()           const;

		void capturePose();
		void applyPose(Transform& transform, float alpha) const;
		void shiftPoses(const glm::vec3& offset);

	private:
		void release();

		PhysicsWorld* world            = nullptr;
		BodyId        id               = INVALID_BODY_ID;
		glm::vec3     previousPosition = {};
		glm::vec3     currentPosition  = {};
		glm::quat     previousRotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		glm::quat     currentRotation  = glm::quat(1.f, 0.f, 0.f, 0.f);
	};

	LUNAR_API void CapturePhysicsPoses(Scene& scene, const FrameTime& frame_time);
	LUNAR_API void ApplyPhysicsPoses(Scene& scene, const FrameTime& frame_time);
	LUNAR_API void ShiftPhysics(Scene& scene, const glm::vec3& offset);
}
