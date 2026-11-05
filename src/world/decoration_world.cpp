#include <lunar/world/decoration_world.hpp>
#include <lunar/world/floating_origin.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/core/scene.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/render/components.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <string_view>
#include <system_error>

namespace lunar::World
{
	namespace
	{
		constexpr uint32_t         CHANGES_FORMAT_VERSION = 1;
		constexpr std::string_view CHANGES_FILE_FORMAT    = "{}.{}.json";
		constexpr std::string_view BATCH_NAME             = "DecorationBatch";
		constexpr std::string_view PROMOTED_NAME          = "Decoration";
		constexpr float            HALF                   = 0.5f;

		nlohmann::json SerializePlaced(const PlacedDecoration& placed)
		{
			return nlohmann::json
			{
				{ "id",       placed.id },
				{ "kind",     placed.kind },
				{ "position", { placed.position.x, placed.position.y, placed.position.z } },
				{ "rotation", { placed.rotation.x, placed.rotation.y, placed.rotation.z, placed.rotation.w } },
				{ "scale",    placed.scale }
			};
		}

		PlacedDecoration DeserializePlaced(const nlohmann::json& json)
		{
			const nlohmann::json& position = json.at("position");
			const nlohmann::json& rotation = json.at("rotation");

			return PlacedDecoration
			{
				.id       = json.at("id").get<DecorationId>(),
				.kind     = json.at("kind").get<std::string>(),
				.position = { position.at(0).get<double>(), position.at(1).get<double>(), position.at(2).get<double>() },
				.rotation = glm::quat(rotation.at(3).get<float>(), rotation.at(0).get<float>(), rotation.at(1).get<float>(), rotation.at(2).get<float>()),
				.scale    = json.at("scale").get<float>()
			};
		}

		float FlatDistance(const glm::vec3& first, const glm::vec3& second)
		{
			return glm::distance(glm::vec2(first.x, first.z), glm::vec2(second.x, second.z));
		}

		bool IsNear(std::span<const glm::vec3> foci, const glm::vec3& position, float radius)
		{
			return std::ranges::any_of(foci, [&](const glm::vec3& focus) { return FlatDistance(focus, position) <= radius; });
		}

		bool HasMoved(const PromotedDecoration& promoted, const glm::dvec3& position, const glm::quat& rotation, const DecorationSettings& settings)
		{
			const float alignment = std::abs(glm::dot(rotation, promoted.rotation));
			return glm::distance(position, promoted.position) > settings.disturbDistance || alignment < std::cos(settings.disturbAngle * HALF);
		}

		glm::mat4 InstanceMatrix(const DecorationInstance& instance)
		{
			return glm::translate(glm::mat4(1.f), instance.position) * glm::mat4_cast(instance.rotation) * glm::scale(glm::mat4(1.f), glm::vec3(instance.scale));
		}

		Render::Bounds Enclose(const std::optional<Render::Bounds>& total, const Render::Bounds& part)
		{
			if (!total.has_value())
				return part;

			return { glm::min(total->min, part.min), glm::max(total->max, part.max) };
		}

		std::vector<std::vector<glm::vec3>> TransformHulls(const std::vector<std::vector<glm::vec3>>& hulls, const glm::mat4& transform)
		{
			std::vector<std::vector<glm::vec3>> transformed;
			for (const std::vector<glm::vec3>& hull : hulls)
			{
				std::vector<glm::vec3>& points = transformed.emplace_back();
				for (const glm::vec3& point : hull)
					points.push_back(glm::vec3(transform * glm::vec4(point, 1.f)));
			}

			return transformed;
		}

		std::vector<Physics::ShapeDesc> HullShapes(const std::vector<std::vector<glm::vec3>>& hulls)
		{
			std::vector<Physics::ShapeDesc> shapes;
			for (const std::vector<glm::vec3>& hull : hulls)
				shapes.push_back({ .geometry = Physics::ConvexHullDesc { hull }, .category = Physics::PROP_CATEGORY });

			return shapes;
		}

		glm::vec3 Centroid(const std::vector<std::vector<glm::vec3>>& hulls)
		{
			glm::vec3 sum   = {};
			size_t    count = 0;
			for (const std::vector<glm::vec3>& hull : hulls)
			{
				for (const glm::vec3& point : hull)
					sum += point;

				count += hull.size();
			}

			return count == 0 ? sum : sum / static_cast<float>(count);
		}

		Render::MeshHandle NearestLod(const DecorationLibrary::Kind& kind)
		{
			const auto lod = std::ranges::find_if(kind.lods, [](const InstanceLod& candidate) { return candidate.mesh != Render::MeshHandle {}; });
			return lod == kind.lods.end() ? Render::MeshHandle {} : lod->mesh;
		}
	}

	nlohmann::json DecorationChanges::Serialize(const DecorationChanges& changes)
	{
		nlohmann::json placed = nlohmann::json::array();
		for (const PlacedDecoration& decoration : changes.placed)
			placed.push_back(SerializePlaced(decoration));

		return nlohmann::json
		{
			{ "formatVersion", CHANGES_FORMAT_VERSION },
			{ "removed",       changes.removed },
			{ "placed",        std::move(placed) }
		};
	}

	std::optional<DecorationChanges> DecorationChanges::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != CHANGES_FORMAT_VERSION)
			return std::nullopt;

		DecorationChanges changes = { .removed = json.at("removed").get<std::vector<DecorationId>>() };
		for (const nlohmann::json& placed : json.at("placed"))
			changes.placed.push_back(DeserializePlaced(placed));

		return changes;
	}

	DecorationWorld::DecorationWorld(Scene&                   scene,
	                                 Render::MeshRegistry&    meshes,
	                                 const DecorationLibrary& library,
	                                 const WorldSettings&     settings,
	                                 Fs::Path                 directory,
	                                 DecorationSettings       decoration_settings) noexcept
		: scene(scene),
		meshes(meshes),
		library(library),
		settings(settings),
		directory(std::move(directory)),
		decorationSettings(decoration_settings)
	{
	}

	DecorationWorld::~DecorationWorld() noexcept
	{
		std::vector<Entity> promoted;
		scene.forEach<PromotedDecoration>([&](Entity entity, const PromotedDecoration&) { promoted.push_back(entity); });

		for (const Entity entity : promoted)
			finish(entity);

		for (auto& [coord, state] : chunks)
			releaseChunk(state);
	}

	void DecorationWorld::update(std::span<const glm::vec3> foci)
	{
		syncChunks();
		updatePromoted(foci);

		for (const glm::vec3& focus : foci)
			promoteNear(focus);

		rebuildDirty();
		updateColliders(foci);
	}

	std::optional<GameObject> DecorationWorld::promote(ChunkCoord coord, DecorationId id)
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end() || promotedIds.contains(id))
			return std::nullopt;

		const auto instance = std::ranges::find(found->second.instances, id, &DecorationInstance::id);
		if (instance == found->second.instances.end())
			return std::nullopt;

		return promoteInstance(coord, *instance);
	}

	std::span<const DecorationInstance> DecorationWorld::findInstances(ChunkCoord coord) const
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end())
			return {};

		return found->second.instances;
	}

	size_t DecorationWorld::getInstanceCount() const
	{
		size_t count = 0;
		for (const auto& [coord, state] : chunks)
			count += state.instances.size();

		return count;
	}

	size_t DecorationWorld::getPromotedCount() const
	{
		return promotedIds.size();
	}

	void DecorationWorld::syncChunks()
	{
		scene.forEach<TerrainChunk, ChunkDecorations>([&](Entity entity, const TerrainChunk& chunk, const ChunkDecorations&) {
			ChunkState& state = chunks[chunk.coord];
			if (state.chunk == entity)
				return;

			releaseChunk(state);
			state = ChunkState { .chunk = entity };
		});

		std::erase_if(chunks, [&](auto& entry) {
			if (scene.hasComponent<ChunkDecorations>(entry.second.chunk))
				return false;

			releaseChunk(entry.second);
			return true;
		});
	}

	void DecorationWorld::updatePromoted(std::span<const glm::vec3> foci)
	{
		std::vector<Entity> finished;
		scene.forEach<PromotedDecoration, Transform>([&](Entity entity, PromotedDecoration& promoted, const Transform& transform) {
			promoted.disturbed = promoted.disturbed || HasMoved(promoted, scene.toAbsolute(transform.position), transform.rotation, decorationSettings);
			if (!IsNear(foci, transform.position, decorationSettings.releaseRadius))
				finished.push_back(entity);
		});

		for (const Entity entity : finished)
			finish(entity);
	}

	void DecorationWorld::promoteNear(const glm::vec3& focus)
	{
		const auto reach = static_cast<int32_t>(std::ceil(decorationSettings.promoteRadius / settings.getChunkSize()));

		for (const ChunkCoord coord : SquareAround(SceneChunkAt(scene, focus, settings), reach))
		{
			const auto found = chunks.find(coord);
			if (found == chunks.end())
				continue;

			const glm::vec3 origin = SceneChunkOrigin(scene, coord, settings);
			for (const DecorationInstance& instance : found->second.instances)
				if (isBreakable(instance.kind) && !promotedIds.contains(instance.id) && FlatDistance(origin + instance.position, focus) <= decorationSettings.promoteRadius)
					promoteInstance(coord, instance);
		}
	}

	void DecorationWorld::rebuildDirty()
	{
		for (auto& [coord, state] : chunks)
		{
			if (!state.dirty)
				continue;

			const ChunkDecorations* decorations = scene.getComponent<ChunkDecorations>(state.chunk);
			releaseChunk(state);
			state.instances = collectInstances(coord, decorations->generated);
			buildBatches(state);
			state.dirty = false;
		}
	}

	void DecorationWorld::updateColliders(std::span<const glm::vec3> foci)
	{
		std::vector<ChunkCoord> focus_chunks;
		for (const glm::vec3& focus : foci)
			focus_chunks.push_back(SceneChunkAt(scene, focus, settings));

		const auto is_needed = [&](ChunkCoord coord) {
			return std::ranges::any_of(focus_chunks, [&](ChunkCoord focus) { return IsInsideSquare(coord, focus, decorationSettings.colliderRadius); });
		};

		for (auto& [coord, state] : chunks)
		{
			if (!state.colliderBuilt || is_needed(coord))
				continue;

			state.collider.reset();
			state.colliderBuilt = false;
		}

		for (const ChunkCoord focus : focus_chunks)
			for (const ChunkCoord coord : SquareAround(focus, decorationSettings.colliderRadius))
				if (const auto found = chunks.find(coord); found != chunks.end() && !found->second.colliderBuilt)
					buildCollider(coord, found->second);
	}

	GameObject DecorationWorld::promoteInstance(ChunkCoord coord, const DecorationInstance& instance)
	{
		const DecorationLibrary::Kind& kind     = *library.find(instance.kind);
		const DecorationChanges&       region   = changesIn(RegionAt(coord, settings));
		const glm::vec3                position = SceneChunkOrigin(scene, coord, settings) + instance.position;

		GameObject object = scene.createGameObject(PROMOTED_NAME);
		object->getTransform() = Transform { .position = position, .rotation = instance.rotation, .scale = glm::vec3(instance.scale) };
		object->addComponent<MeshRenderer>(NearestLod(kind));
		object->addComponent<PromotedDecoration>(PromotedDecoration {
			.id       = instance.id,
			.kind     = instance.kind,
			.chunk    = coord,
			.placed   = std::ranges::find(region.placed, instance.id, &PlacedDecoration::id) != region.placed.end(),
			.position = scene.toAbsolute(position),
			.rotation = instance.rotation,
			.scale    = instance.scale
		});

		if (!kind.colliders.empty())
		{
			const std::vector<std::vector<glm::vec3>> hulls  = TransformHulls(kind.colliders, glm::scale(glm::mat4(1.f), glm::vec3(instance.scale)));
			const std::vector<Physics::ShapeDesc>     shapes = HullShapes(hulls);

			object->addComponent<Physics::RigidBody>(scene, Physics::BodyDesc {
				.position     = position,
				.rotation     = instance.rotation,
				.type         = Physics::BodyType::eDynamic,
				.shapes       = shapes,
				.massOverride = kind.type.mass > 0.f ? std::optional(Physics::MassDesc { kind.type.mass, Centroid(hulls) }) : std::nullopt,
				.startAsleep  = true
			});
		}

		promotedIds.insert(instance.id);
		markDirty(coord);
		return object;
	}

	void DecorationWorld::finish(Entity entity)
	{
		const PromotedDecoration promoted = *scene.getComponent<PromotedDecoration>(entity);
		if (promoted.disturbed)
			retire(promoted, *scene.getComponent<Transform>(entity));

		promotedIds.erase(promoted.id);
		markDirty(promoted.chunk);
		scene.removeComponent<PromotedDecoration>(entity);
		scene.destroyEntity(entity);
	}

	void DecorationWorld::retire(const PromotedDecoration& promoted, const Transform& transform)
	{
		const glm::dvec3  position    = scene.toAbsolute(transform.position);
		const ChunkCoord  resting     = ChunkAt(position.x, position.z, settings);
		const RegionCoord source      = RegionAt(promoted.chunk, settings);
		const RegionCoord destination = RegionAt(resting, settings);

		DecorationChanges& origin = changesIn(source);
		if (promoted.placed)
			std::erase_if(origin.placed, [&](const PlacedDecoration& placed) { return placed.id == promoted.id; });
		else
			origin.removed.push_back(promoted.id);

		changesIn(destination).placed.push_back({ promoted.id, library.find(promoted.kind)->type.name, position, transform.rotation, promoted.scale });

		save(source);
		if (destination != source)
			save(destination);

		markDirty(resting);
	}

	std::vector<DecorationInstance> DecorationWorld::collectInstances(ChunkCoord coord, std::span<const DecorationInstance> generated)
	{
		const DecorationChanges& region = changesIn(RegionAt(coord, settings));
		const glm::dvec3         origin = scene.toAbsolute(SceneChunkOrigin(scene, coord, settings));

		std::vector<DecorationInstance> instances;
		for (const DecorationInstance& instance : generated)
			if (!promotedIds.contains(instance.id) && std::ranges::find(region.removed, instance.id) == region.removed.end())
				instances.push_back(instance);

		for (const PlacedDecoration& placed : region.placed)
		{
			const std::optional<DecorationKind> kind = library.indexOf(placed.kind);
			if (kind.has_value() && !promotedIds.contains(placed.id) && ChunkAt(placed.position.x, placed.position.z, settings) == coord)
				instances.push_back({ placed.id, *kind, glm::vec3(placed.position - origin), placed.rotation, placed.scale });
		}

		return instances;
	}

	void DecorationWorld::buildBatches(ChunkState& state)
	{
		std::vector<std::vector<Render::InstanceData>> instances(library.size());
		std::vector<std::optional<Render::Bounds>>     bounds(library.size());

		for (const DecorationInstance& instance : state.instances)
		{
			const DecorationLibrary::Kind* kind = library.find(instance.kind);
			if (kind == nullptr)
				continue;

			const glm::quat& rotation = instance.rotation;
			instances[instance.kind].push_back({ instance.position, instance.scale, glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w) });
			bounds[instance.kind] = Enclose(bounds[instance.kind], kind->bounds.transformed(InstanceMatrix(instance)));
		}

		GameObject chunk = GameObject(&scene, state.chunk);
		for (size_t kind = 0; kind < instances.size(); kind++)
		{
			if (instances[kind].empty())
				continue;

			const Render::InstanceSetHandle set   = meshes.createInstances(instances[kind]);
			GameObject                      batch = chunk.createChildObject(BATCH_NAME);

			batch->addComponent<InstanceBatch>(library.find(static_cast<DecorationKind>(kind))->lods, set, *bounds[kind]);
			state.batches.push_back(batch.getEntity());
			state.sets.push_back(set);
		}
	}

	void DecorationWorld::buildCollider(ChunkCoord coord, ChunkState& state)
	{
		state.colliderBuilt = true;

		std::vector<std::vector<glm::vec3>> hulls;
		for (const DecorationInstance& instance : state.instances)
		{
			const DecorationLibrary::Kind* kind = library.find(instance.kind);
			if (kind == nullptr || kind->type.breakable)
				continue;

			for (std::vector<glm::vec3>& hull : TransformHulls(kind->colliders, InstanceMatrix(instance)))
				hulls.push_back(std::move(hull));
		}

		if (hulls.empty())
			return;

		const std::vector<Physics::ShapeDesc> shapes = HullShapes(hulls);
		state.collider.emplace(scene, Physics::BodyDesc { .position = SceneChunkOrigin(scene, coord, settings), .shapes = shapes });
	}

	void DecorationWorld::releaseChunk(ChunkState& state)
	{
		for (const Entity batch : state.batches)
			scene.destroyEntity(batch);

		for (const Render::InstanceSetHandle set : state.sets)
			meshes.destroy(set);

		state.batches.clear();
		state.sets.clear();
		state.collider.reset();
		state.colliderBuilt = false;
	}

	void DecorationWorld::markDirty(ChunkCoord coord)
	{
		const auto found = chunks.find(coord);
		if (found != chunks.end())
			found->second.dirty = true;
	}

	bool DecorationWorld::isBreakable(DecorationKind kind) const
	{
		const DecorationLibrary::Kind* found = library.find(kind);
		return found != nullptr && found->type.breakable && !found->colliders.empty();
	}

	DecorationChanges& DecorationWorld::changesIn(RegionCoord region)
	{
		const auto found = changes.find(region);
		if (found != changes.end())
			return found->second;

		std::optional<DecorationChanges> loaded = directory.empty() ? std::nullopt : Fs::LoadJson<DecorationChanges>(directory / std::format(CHANGES_FILE_FORMAT, region.x, region.z));
		return changes.emplace(region, std::move(loaded).value_or(DecorationChanges {})).first->second;
	}

	void DecorationWorld::save(RegionCoord region) const
	{
		if (directory.empty())
			return;

		std::error_code error;
		std::filesystem::create_directories(directory, error);

		Fs::JsonFile file;
		file.content = DecorationChanges::Serialize(changes.at(region));
		file.toFile(directory / std::format(CHANGES_FILE_FORMAT, region.x, region.z));
	}
}
