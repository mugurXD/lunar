#pragma once
#include <lunar/api.hpp>
#include <lunar/core/time.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <lunar/physics/reactphysics.hpp>

#include <cstdint>
#include <span>
#include <vector>

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

	class LUNAR_API RigidBody
	{
	public:
		RigidBody(Scene& scene, const glm::vec3& position, const glm::quat& rotation, BodyType type) noexcept;
		~RigidBody() noexcept;

		RigidBody(RigidBody&& other) noexcept;
		RigidBody& operator=(RigidBody&& other) noexcept;

		RigidBody(const RigidBody&)            = delete;
		RigidBody& operator=(const RigidBody&) = delete;

		rp3d::Collider* addBox(const glm::vec3& half_extents, const glm::vec3& offset, uint16_t category);
		rp3d::Collider* addHeightfield(const HeightfieldDesc& desc, uint16_t category);
		rp3d::Collider* addTriangleMesh(const TriangleMeshDesc& desc, uint16_t category);
		void            setMass(float mass, const glm::vec3& center_of_mass);

		rp3d::RigidBody&    getBody();
		rp3d::PhysicsWorld& getWorld();

		void capturePose();
		void applyPose(Transform& transform, float alpha) const;

	private:
		void release();

		rp3d::PhysicsCommon*               common           = nullptr;
		rp3d::PhysicsWorld*                world            = nullptr;
		rp3d::RigidBody*                   body             = nullptr;
		std::vector<rp3d::CollisionShape*> shapes           = {};
		std::vector<rp3d::TriangleMesh*>   triangleMeshes   = {};
		glm::vec3                          previousPosition = {};
		glm::vec3                          currentPosition  = {};
		glm::quat                          previousRotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		glm::quat                          currentRotation  = glm::quat(1.f, 0.f, 0.f, 0.f);
	};

	LUNAR_API void CapturePhysicsPoses(Scene& scene, const FrameTime& frame_time);
	LUNAR_API void ApplyPhysicsPoses(Scene& scene, const FrameTime& frame_time);
}
