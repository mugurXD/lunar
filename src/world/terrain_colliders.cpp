#include <lunar/world/terrain_colliders.hpp>
#include <lunar/world/floating_origin.hpp>
#include <lunar/core/scene.hpp>

#include <algorithm>
#include <vector>

namespace lunar::World
{
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
			focus_chunks.push_back(SceneChunkAt(scene, position, settings));

		std::erase_if(colliders, [&](const auto& entry) { return !isNeeded(entry.first, focus_chunks) || entry.second.revision != terrain.getRevision(entry.first); });

		for (const ChunkCoord focus : focus_chunks)
		{
			for (const ChunkCoord coord : chunksAround(focus))
			{
				const Heightmap* heightmap = terrain.findHeightmap(coord);
				if (heightmap != nullptr && !colliders.contains(coord))
					create(coord, *heightmap);
			}
		}
	}

	void TerrainColliders::clear()
	{
		colliders.clear();
	}

	bool TerrainColliders::isReady(const glm::vec3& position) const
	{
		return std::ranges::all_of(chunksAround(SceneChunkAt(scene, position, settings)), [&](ChunkCoord coord) { return colliders.contains(coord); });
	}

	bool TerrainColliders::isTerrainLoaded(const glm::vec3& position) const
	{
		return std::ranges::all_of(chunksAround(SceneChunkAt(scene, position, settings)), [&](ChunkCoord coord) { return terrain.findHeightmap(coord) != nullptr; });
	}

	size_t TerrainColliders::getColliderCount() const
	{
		return colliders.size();
	}

	bool TerrainColliders::isNeeded(ChunkCoord coord, std::span<const ChunkCoord> focus_chunks) const
	{
		return std::ranges::any_of(focus_chunks, [&](ChunkCoord focus) { return IsInsideSquare(coord, focus, radiusChunks); });
	}

	std::vector<ChunkCoord> TerrainColliders::chunksAround(ChunkCoord focus) const
	{
		std::vector<ChunkCoord> chunks;
		for (int32_t offset_z = -radiusChunks; offset_z <= radiusChunks; offset_z++)
			for (int32_t offset_x = -radiusChunks; offset_x <= radiusChunks; offset_x++)
				chunks.push_back({ focus.x + offset_x, focus.z + offset_z });

		return chunks;
	}

	void TerrainColliders::create(ChunkCoord coord, const Heightmap& heightmap)
	{
		const auto         samples_per_side = static_cast<int32_t>(settings.chunkQuads + 1);
		std::vector<float> heights;
		heights.reserve(static_cast<size_t>(samples_per_side) * samples_per_side);

		for (int32_t z = 0; z < samples_per_side; z++)
			for (int32_t x = 0; x < samples_per_side; x++)
				heights.push_back(heightmap.sample(x, z));

		const std::span<const DressedMesh>  solids = terrain.findSolids(coord);
		std::vector<std::vector<glm::vec3>> solid_positions(solids.size());
		std::vector<Physics::ShapeDesc>     shapes =
		{
			{ .geometry = Physics::HeightfieldDesc { .heights = heights, .samplesPerSide = static_cast<uint32_t>(samples_per_side), .spacing = settings.vertexSpacing }, .category = Physics::TERRAIN_CATEGORY }
		};

		for (size_t index = 0; index < solids.size(); index++)
		{
			const Render::MeshData& mesh = solids[index].mesh;
			solid_positions[index].reserve(mesh.vertices.size());
			for (const Render::Vertex& vertex : mesh.vertices)
				solid_positions[index].push_back(vertex.position);

			shapes.push_back({ .geometry = Physics::TriangleMeshDesc { .vertices = solid_positions[index], .indices = mesh.indices }, .category = solids[index].colliderCategory });
		}

		Physics::RigidBody body(scene, { .position = SceneChunkOrigin(scene, coord, settings), .shapes = shapes });
		colliders.emplace(coord, ChunkCollider { std::move(body), terrain.getRevision(coord) });
	}
}
