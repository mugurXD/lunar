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
		constexpr std::string_view CHUNK_NAME_FORMAT = "TerrainChunk {},{}";
	}

	TerrainWorld::TerrainWorld(Scene&                scene,
	                           JobSystem&            jobs,
	                           Render::MeshRegistry& meshes,
	                           ChunkSource&          source,
	                           const WorldSettings&  settings) noexcept
		: scene(scene),
		jobs(jobs),
		meshes(meshes),
		source(source),
		settings(settings)
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
		const JobHandle job = jobs.submit(std::move(work), [this, coord](ChunkData data) { onChunkGenerated(coord, std::move(data)); });

		chunks.emplace(coord, LoadedChunk { .job = job });
		pendingCount++;
	}

	void TerrainWorld::onChunkGenerated(ChunkCoord coord, ChunkData data)
	{
		const auto found = chunks.find(coord);
		if (found == chunks.end())
			return;

		LoadedChunk& chunk = found->second;
		chunk.mesh         = meshes.create(data.mesh);
		chunk.heightmap    = std::move(data.heightmap);
		chunk.object       = scene.createGameObject(std::format(CHUNK_NAME_FORMAT, coord.x, coord.z));

		chunk.object->getTransform().position = ChunkOrigin(coord, settings);
		chunk.object->addComponent<MeshRenderer>(chunk.mesh);
		chunk.object->addComponent<TerrainChunk>(coord);

		pendingCount--;
	}

	void TerrainWorld::unloadChunk(const LoadedChunk& chunk)
	{
		if (chunk.object == nullptr)
		{
			jobs.cancel(chunk.job);
			pendingCount--;
			return;
		}

		meshes.destroy(chunk.mesh);

		GameObject object = chunk.object;
		object.destroy();
	}
}
