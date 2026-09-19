#include <lunar/world/terrain_colliders.hpp>
#include <lunar/core/scene.hpp>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace lunar::World
{
	namespace
	{
		bool IsInsideSquare(ChunkCoord coord, ChunkCoord center, int32_t radius)
		{
			return std::abs(coord.x - center.x) <= radius && std::abs(coord.z - center.z) <= radius;
		}
	}

	TerrainColliders::TerrainColliders(Scene& scene, const TerrainWorld& terrain, const WorldSettings& settings, int32_t radius_chunks) noexcept
		: scene(scene),
		terrain(terrain),
		settings(settings),
		radiusChunks(radius_chunks)
	{
	}

	void TerrainColliders::update(std::span<const glm::vec3> focus_positions)
	{
		std::vector<ChunkCoord> focus_chunks;
		for (const glm::vec3& position : focus_positions)
			focus_chunks.push_back(ChunkAt(position, settings));

		std::erase_if(colliders, [&](const auto& entry) { return !isNeeded(entry.first, focus_chunks); });

		for (const ChunkCoord focus : focus_chunks)
		{
			for (int32_t offset_z = -radiusChunks; offset_z <= radiusChunks; offset_z++)
			{
				for (int32_t offset_x = -radiusChunks; offset_x <= radiusChunks; offset_x++)
				{
					const ChunkCoord coord     = { focus.x + offset_x, focus.z + offset_z };
					const Heightmap* heightmap = terrain.findHeightmap(coord);
					if (heightmap != nullptr && !colliders.contains(coord))
						create(coord, *heightmap);
				}
			}
		}
	}

	void TerrainColliders::clear()
	{
		colliders.clear();
	}

	bool TerrainColliders::isReady(const glm::vec3& position) const
	{
		const ChunkCoord focus = ChunkAt(position, settings);

		for (int32_t offset_z = -radiusChunks; offset_z <= radiusChunks; offset_z++)
			for (int32_t offset_x = -radiusChunks; offset_x <= radiusChunks; offset_x++)
				if (!colliders.contains({ focus.x + offset_x, focus.z + offset_z }))
					return false;

		return true;
	}

	size_t TerrainColliders::getColliderCount() const
	{
		return colliders.size();
	}

	bool TerrainColliders::isNeeded(ChunkCoord coord, std::span<const ChunkCoord> focus_chunks) const
	{
		return std::ranges::any_of(focus_chunks, [&](ChunkCoord focus) { return IsInsideSquare(coord, focus, radiusChunks); });
	}

	void TerrainColliders::create(ChunkCoord coord, const Heightmap& heightmap)
	{
		const auto         samples_per_side = static_cast<int32_t>(settings.chunkQuads + 1);
		std::vector<float> heights;
		heights.reserve(static_cast<size_t>(samples_per_side) * samples_per_side);

		for (int32_t z = 0; z < samples_per_side; z++)
			for (int32_t x = 0; x < samples_per_side; x++)
				heights.push_back(heightmap.sample(x, z));

		Physics::RigidBody body(scene, ChunkOrigin(coord, settings), glm::quat(1.f, 0.f, 0.f, 0.f), Physics::BodyType::eStatic);
		body.addHeightfield({ .heights = heights, .samplesPerSide = static_cast<uint32_t>(samples_per_side), .spacing = settings.vertexSpacing }, Physics::TERRAIN_CATEGORY);
		colliders.emplace(coord, std::move(body));
	}
}
