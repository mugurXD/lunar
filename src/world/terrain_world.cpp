#include <lunar/world/terrain_world.hpp>
#include <lunar/render/components.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

namespace lunar::World
{
	namespace
	{
		constexpr std::string_view CHUNK_NAME_FORMAT = "TerrainChunk {},{}";

		int64_t DistanceSquared(ChunkCoord a, ChunkCoord b)
		{
			const int64_t delta_x = static_cast<int64_t>(a.x) - b.x;
			const int64_t delta_z = static_cast<int64_t>(a.z) - b.z;
			return delta_x * delta_x + delta_z * delta_z;
		}

		bool IsWithin(ChunkCoord coord, ChunkCoord center, int32_t radius)
		{
			return DistanceSquared(coord, center) <= static_cast<int64_t>(radius) * radius;
		}
	}

	TerrainWorld::TerrainWorld(Scene&                                  scene,
	                           JobSystem&                              jobs,
	                           Render::MeshRegistry&                   meshes,
	                           std::shared_ptr<const TerrainGenerator> generator,
	                           const TerrainSettings&                  settings) noexcept
		: scene(scene),
		jobs(jobs),
		meshes(meshes),
		generator(std::move(generator)),
		settings(settings)
	{
	}

	TerrainWorld::~TerrainWorld() noexcept
	{
		for (const auto& [coord, chunk] : chunks)
			unloadChunk(chunk);
	}

	void TerrainWorld::update(const glm::vec3& focus_position)
	{
		const ChunkCoord center = ChunkAt(focus_position, settings);

		unloadDistantChunks(center);
		requestMissingChunks(center);
	}

	size_t TerrainWorld::getLoadedChunkCount() const
	{
		return chunks.size() - pendingCount;
	}

	size_t TerrainWorld::getPendingChunkCount() const
	{
		return pendingCount;
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
		if (pendingCount >= settings.maxJobsInFlight)
			return;

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

		const auto request_budget = static_cast<std::ptrdiff_t>(settings.maxJobsInFlight - pendingCount);
		for (const ChunkCoord coord : missing | std::views::take(request_budget))
			requestChunk(coord);
	}

	void TerrainWorld::requestChunk(ChunkCoord coord)
	{
		const JobHandle job = jobs.submit(
			[generator = generator, settings = settings, coord] { return GenerateChunk(*generator, coord, settings); },
			[this, coord](ChunkData data) { onChunkGenerated(coord, std::move(data)); }
		);

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
