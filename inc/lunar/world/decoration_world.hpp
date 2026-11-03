#pragma once
#include <lunar/api.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/physics/rigid_body.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/world/decoration.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lunar
{
	class Scene;
}

namespace lunar::World
{
	struct LUNAR_API DecorationSettings
	{
		float   promoteRadius   = 40.f;
		float   releaseRadius   = 55.f;
		int32_t colliderRadius  = 1;
		float   disturbDistance = 0.2f;
		float   disturbAngle    = 0.1f;
	};

	struct LUNAR_API PromotedDecoration
	{
		DecorationId   id        = 0;
		DecorationKind kind      = 0;
		ChunkCoord     chunk     = {};
		bool           placed    = false;
		glm::dvec3     position  = {};
		glm::quat      rotation  = glm::quat(1.f, 0.f, 0.f, 0.f);
		float          scale     = 1.f;
		bool           disturbed = false;
	};

	struct LUNAR_API PlacedDecoration
	{
		DecorationId id       = 0;
		std::string  kind     = {};
		glm::dvec3   position = {};
		glm::quat    rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		float        scale    = 1.f;

		bool operator==(const PlacedDecoration&) const = default;
	};

	struct LUNAR_API DecorationChanges
	{
		std::vector<DecorationId>     removed = {};
		std::vector<PlacedDecoration> placed  = {};

		bool operator==(const DecorationChanges&) const = default;

		static nlohmann::json                   Serialize(const DecorationChanges& changes);
		static std::optional<DecorationChanges> Deserialize(const nlohmann::json& json);
	};

	class LUNAR_API DecorationWorld
	{
	public:
		DecorationWorld(Scene&                   scene,
		                Render::MeshRegistry&    meshes,
		                const DecorationLibrary& library,
		                const WorldSettings&     settings,
		                Fs::Path                 directory,
		                DecorationSettings       decoration_settings = {}) noexcept;
		~DecorationWorld() noexcept;

		DecorationWorld(const DecorationWorld&)            = delete;
		DecorationWorld& operator=(const DecorationWorld&) = delete;

		void                                update(std::span<const glm::vec3> foci);
		std::optional<GameObject>           promote(ChunkCoord coord, DecorationId id);
		std::span<const DecorationInstance> findInstances(ChunkCoord coord) const;
		size_t                              getInstanceCount()              const;
		size_t                              getPromotedCount()              const;

	private:
		struct ChunkState
		{
			Entity                                 chunk         = nullptr;
			std::vector<DecorationInstance>        instances     = {};
			std::vector<Entity>                    batches       = {};
			std::vector<Render::InstanceSetHandle> sets          = {};
			std::optional<Physics::RigidBody>      collider      = std::nullopt;
			bool                                   colliderBuilt = false;
			bool                                   dirty         = true;
		};

		void                            syncChunks();
		void                            updatePromoted(std::span<const glm::vec3> foci);
		void                            promoteNear(const glm::vec3& focus);
		void                            rebuildDirty();
		void                            updateColliders(std::span<const glm::vec3> foci);
		GameObject                      promoteInstance(ChunkCoord coord, const DecorationInstance& instance);
		void                            finish(Entity entity);
		void                            retire(const PromotedDecoration& promoted, const Transform& transform);
		std::vector<DecorationInstance> collectInstances(ChunkCoord coord, std::span<const DecorationInstance> generated);
		void                            buildBatches(ChunkState& state);
		void                            buildCollider(ChunkCoord coord, ChunkState& state);
		void                            releaseChunk(ChunkState& state);
		void                            markDirty(ChunkCoord coord);
		bool                            isBreakable(DecorationKind kind) const;
		DecorationChanges&              changesIn(RegionCoord region);
		void                            save(RegionCoord region) const;

		Scene&                                                            scene;
		Render::MeshRegistry&                                             meshes;
		const DecorationLibrary&                                          library;
		WorldSettings                                                     settings;
		Fs::Path                                                          directory;
		DecorationSettings                                                decorationSettings;
		std::unordered_map<ChunkCoord, ChunkState, GridCoordHash>         chunks;
		std::unordered_map<RegionCoord, DecorationChanges, GridCoordHash> changes;
		std::unordered_set<DecorationId>                                  promotedIds;
	};
}
