#include <lunar/world/terrain_world.hpp>
#include <lunar/render/components.hpp>

#include <algorithm>
#include <format>
#include <optional>
#include <string_view>
#include <vector>

namespace lunar::World
{
	namespace
	{
		constexpr std::string_view CHUNK_NAME_FORMAT      = "TerrainChunk {},{}";
		constexpr std::string_view DRESSING_NAME_FORMAT = "TerrainDressing {},{}";
	}

	TerrainWorld::TerrainWorld(Scene&                 scene,
	                           JobSystem&             jobs,
	                           Render::MeshRegistry&  meshes,
	                           ChunkSource&           source,
	                           const WorldSettings&   settings,
	                           Render::MaterialHandle material) noexcept
		: scene(scene),
		jobs(jobs),
		meshes(meshes),
		source(source),
		settings(settings),
		material(material)
	{
	}

	TerrainWorld::~TerrainWorld() noexcept
	{
		reload();
	}

	void TerrainWorld::update(const glm::vec3& focus_position)
	{
		const ChunkCoord center = ChunkAt(focus_position, settings);

		unloadDistantChunks(center);
		requestMissingChunks(center);
	}

	void TerrainWorld::reload()
	{
		for (const auto& [coord, chunk] : chunks)
			unloadChunk(chunk);

		chunks.clear();
	}

	size_t TerrainWorld::getLoadedChunkCount() const
	{
		return chunks.size() - pendingCount;
	}

	size_t TerrainWorld::getPendingChunkCount() const
	{
		return pendingCount;
	}

	void TerrainWorld::refresh(std::span<const ChunkCoord> coords)
	{
		for (const ChunkCoord coord : coords)
		{
			const auto found = chunks.find(coord);
			if (found == chunks.end())
				continue;

			std::optional<ChunkWork> work = source.prepareChunk(coord);
			if (!work.has_value())
				continue;

			jobs.cancel(found->second.job);
			found->second.job = submitChunk(coord, std::move(*work));
		}
	}

	uint64_t TerrainWorld::getRevision(ChunkCoord coord) const
	{
		const auto found = chunks.find(coord);
		return found == chunks.end() ? 0 : found->second.revision;
	}

	std::span<const DressedMesh> TerrainWorld::findSolids(ChunkCoord coord) const
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end())
			return {};

		return found->second.solids;
	}

	const Heightmap* TerrainWorld::findHeightmap(ChunkCoord coord) const
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end() || found->second.heightmap.heights.empty())
			return nullptr;

		return &found->second.heightmap;
	}

	void TerrainWorld::unloadDistantChunks(ChunkCoord center)
	{
		const int32_t unload_radius = settings.viewRadius + settings.unloadMargin;

		std::erase_if(chunks, [&](const auto& entry) {
			if (IsWithin(entry.first, center, unload_radius))
				return false;

			unloadChunk(entry.second);
			return true;
		});
	}

	void TerrainWorld::requestMissingChunks(ChunkCoord center)
	{
		std::vector<ChunkCoord> missing;
		for (int32_t offset_z = -settings.viewRadius; offset_z <= settings.viewRadius; offset_z++)
		{
			for (int32_t offset_x = -settings.viewRadius; offset_x <= settings.viewRadius; offset_x++)
			{
				const ChunkCoord coord = { center.x + offset_x, center.z + offset_z };
				if (IsWithin(coord, center, settings.viewRadius) && !chunks.contains(coord))
					missing.push_back(coord);
			}
		}

		std::ranges::sort(missing, {}, [&](ChunkCoord coord) { return DistanceSquared(coord, center); });

		for (auto next = missing.begin(); next != missing.end() && pendingCount < settings.maxJobsInFlight; ++next)
		{
			std::optional<ChunkWork> work = source.prepareChunk(*next);
			if (work.has_value())
				requestChunk(*next, std::move(*work));
		}
	}

	void TerrainWorld::requestChunk(ChunkCoord coord, ChunkWork work)
	{
		chunks.emplace(coord, LoadedChunk { .job = submitChunk(coord, std::move(work)) });
		pendingCount++;
	}

	JobHandle TerrainWorld::submitChunk(ChunkCoord coord, ChunkWork work)
	{
		return jobs.submit(std::move(work), [this, coord](ChunkData data) { onChunkGenerated(coord, std::move(data)); });
	}

	void TerrainWorld::onChunkGenerated(ChunkCoord coord, ChunkData data)
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end())
			return;

		LoadedChunk& chunk     = found->second;
		const bool   refreshed = chunk.object != nullptr;
		if (refreshed)
		{
			destroyResources(chunk);
			chunk.dressingMeshes.clear();
			chunk.solids.clear();
		}

		chunk.mesh         = meshes.create(data.mesh);
		chunk.heightmap    = std::move(data.heightmap);
		chunk.object       = scene.createGameObject(std::format(CHUNK_NAME_FORMAT, coord.x, coord.z));

		chunk.object->getTransform().position = ChunkOrigin(coord, settings);
		chunk.object->addComponent<MeshRenderer>(chunk.mesh, true, false, material);
		chunk.object->addComponent<TerrainChunk>(coord);

		for (DressedMesh& dressed : data.dressing)
		{
			if (dressed.mesh.vertices.empty())
				continue;

			chunk.dressingMeshes.push_back(meshes.create(dressed.mesh));

			GameObject dressing = chunk.object->createChildObject(std::format(DRESSING_NAME_FORMAT, coord.x, coord.z));
			dressing->addComponent<MeshRenderer>(chunk.dressingMeshes.back(), true, dressed.translucent);

			if (dressed.colliderCategory != 0)
				chunk.solids.push_back(std::move(dressed));
		}

		chunk.revision = nextRevision++;
		if (!refreshed)
			pendingCount--;
	}

	void TerrainWorld::unloadChunk(const LoadedChunk& chunk)
	{
		jobs.cancel(chunk.job);

		if (chunk.object == nullptr)
		{
			pendingCount--;
			return;
		}

		destroyResources(chunk);
	}

	void TerrainWorld::destroyResources(const LoadedChunk& chunk)
	{
		meshes.destroy(chunk.mesh);

		for (const Render::MeshHandle dressing : chunk.dressingMeshes)
			meshes.destroy(dressing);

		GameObject object = chunk.object;
		object.destroy();
	}
}
