#pragma once
#include <lunar/physics/physics_world.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace lunar::Physics
{
	constexpr JPH::ObjectLayer STATIC_LAYER = 0;
	constexpr JPH::ObjectLayer MOVING_LAYER = 1;

	struct CollisionLayers
	{
		CollisionLayers() noexcept;

		JPH::BroadPhaseLayerInterfaceTable broadPhase;
		JPH::ObjectLayerPairFilterTable    objectPairs;
	};

	struct PhysicsBackend
	{
		PhysicsBackend(JPH::JobSystem& job_system) noexcept;

		JPH::BodyInterface& bodies()
		{
			return system.GetBodyInterface();
		}

		CollisionLayers                         layers;
		JPH::ObjectVsBroadPhaseLayerFilterTable objectVsBroadPhase;
		JPH::TempAllocatorImpl                  tempAllocator;
		JPH::JobSystem&                         jobSystem;
		JPH::PhysicsSystem                      system;
	};
}
