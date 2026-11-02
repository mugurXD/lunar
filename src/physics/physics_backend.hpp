#pragma once
#include <lunar/physics/physics_world.hpp>
#include "reactphysics.hpp"

#include <unordered_map>
#include <vector>

namespace lunar::Physics
{
	struct BodyRecord
	{
		rp3d::RigidBody*                   body           = nullptr;
		std::vector<rp3d::CollisionShape*> shapes         = {};
		std::vector<rp3d::TriangleMesh*>   triangleMeshes = {};
	};

	struct PhysicsBackend
	{
		rp3d::PhysicsCommon&                   common;
		rp3d::PhysicsWorld*                    world  = nullptr;
		std::unordered_map<BodyId, BodyRecord> bodies = {};

		rp3d::RigidBody& body(BodyId id) const
		{
			return *bodies.at(id).body;
		}
	};
}
