#include <lunar/physics/physics_world.hpp>
#include <lunar/debug.hpp>
#include "conversions.hpp"
#include "physics_backend.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/RegisterTypes.h>

#include <array>
#include <cstdarg>
#include <cstdio>

namespace lunar::Physics
{
	namespace
	{
		constexpr uint32_t             LAYER_COUNT              = 2;
		constexpr JPH::BroadPhaseLayer STATIC_BROAD_PHASE_LAYER = JPH::BroadPhaseLayer(0);
		constexpr JPH::BroadPhaseLayer MOVING_BROAD_PHASE_LAYER = JPH::BroadPhaseLayer(1);
		constexpr uint32_t             MAX_BODIES               = 16384;
		constexpr uint32_t             AUTOMATIC_BODY_MUTEXES   = 0;
		constexpr uint32_t             MAX_BODY_PAIRS           = 16384;
		constexpr uint32_t             MAX_CONTACT_CONSTRAINTS  = 8192;
		constexpr uint32_t             TEMP_ALLOCATOR_BYTES     = 32 * 1024 * 1024;
		constexpr int                  COLLISION_STEPS          = 1;
		constexpr size_t               TRACE_BUFFER_SIZE        = 1024;

		void TraceToLog(const char* format, ...)
		{
			std::array<char, TRACE_BUFFER_SIZE> message = {};
			va_list                             arguments;
			va_start(arguments, format);
			std::vsnprintf(message.data(), message.size(), format, arguments);
			va_end(arguments);

			DEBUG_WARN("Jolt: {}", message.data());
		}

		class JoltRuntime
		{
		public:
			JoltRuntime() noexcept
			{
				JPH::RegisterDefaultAllocator();
				JPH::Trace = TraceToLog;

				factory                 = std::make_unique<JPH::Factory>();
				JPH::Factory::sInstance = factory.get();
				JPH::RegisterTypes();
				jobSystem.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
			}

			~JoltRuntime() noexcept
			{
				JPH::UnregisterTypes();
				JPH::Factory::sInstance = nullptr;
			}

			JoltRuntime(const JoltRuntime&)            = delete;
			JoltRuntime& operator=(const JoltRuntime&) = delete;

			JPH::JobSystemThreadPool jobSystem;

		private:
			std::unique_ptr<JPH::Factory> factory = nullptr;
		};

		JoltRuntime& Runtime()
		{
			static JoltRuntime runtime;
			return runtime;
		}
	}

	CollisionLayers::CollisionLayers() noexcept
		: broadPhase(LAYER_COUNT, LAYER_COUNT),
		objectPairs(LAYER_COUNT)
	{
		broadPhase.MapObjectToBroadPhaseLayer(STATIC_LAYER, STATIC_BROAD_PHASE_LAYER);
		broadPhase.MapObjectToBroadPhaseLayer(MOVING_LAYER, MOVING_BROAD_PHASE_LAYER);
		objectPairs.EnableCollision(STATIC_LAYER, MOVING_LAYER);
		objectPairs.EnableCollision(MOVING_LAYER, MOVING_LAYER);
	}

	PhysicsBackend::PhysicsBackend(JPH::JobSystem& job_system) noexcept
		: objectVsBroadPhase(layers.broadPhase, LAYER_COUNT, layers.objectPairs, LAYER_COUNT),
		tempAllocator(TEMP_ALLOCATOR_BYTES),
		jobSystem(job_system)
	{
		system.Init(MAX_BODIES, AUTOMATIC_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS, layers.broadPhase, objectVsBroadPhase, layers.objectPairs);
	}

	PhysicsWorld::PhysicsWorld() noexcept
		: backend(std::make_unique<PhysicsBackend>(Runtime().jobSystem))
	{
	}

	PhysicsWorld::~PhysicsWorld() noexcept = default;

	void PhysicsWorld::step(float delta_time)
	{
		const JPH::EPhysicsUpdateError error = backend->system.Update(delta_time, COLLISION_STEPS, &backend->tempAllocator, &backend->jobSystem);
		if (error != JPH::EPhysicsUpdateError::None)
			DEBUG_WARN("Physics update ran out of capacity (error flags {})", static_cast<uint32_t>(error));
	}

	void PhysicsWorld::shift(const glm::vec3& offset)
	{
		JPH::BodyIDVector body_ids;
		backend->system.GetBodies(body_ids);

		JPH::BodyInterface& bodies = backend->bodies();
		for (const JPH::BodyID& body : body_ids)
			bodies.SetPosition(body, bodies.GetPosition(body) + ToPhysics(offset), JPH::EActivation::DontActivate);

		backend->system.OptimizeBroadPhase();
	}
}
