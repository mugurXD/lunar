#pragma once
#include <lunar/api.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/world_settings.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace lunar::World
{
	constexpr int32_t HEIGHTMAP_BORDER     = 1;
	constexpr float   TERRAIN_VERTEX_ALPHA = 1.f;

	struct LUNAR_API TerrainChunk
	{
		ChunkCoord coord = {};
	};

	struct LUNAR_API Heightmap
	{
		uint32_t           samplesPerSide = 0;
		std::vector<float> heights        = {};

		float sample(int32_t x, int32_t z) const;

		bool operator==(const Heightmap&) const = default;
	};

	struct LUNAR_API ChunkData
	{
		Heightmap        heightmap  = {};
		Render::MeshData mesh       = {};
		Render::MeshData decoration = {};
		Render::MeshData water      = {};
	};

	using ChunkWork = std::move_only_function<ChunkData()>;

	class LUNAR_API ChunkSource
	{
	public:
		ChunkSource()          noexcept = default;
		virtual ~ChunkSource() noexcept = default;

		ChunkSource(const ChunkSource&)            = delete;
		ChunkSource& operator=(const ChunkSource&) = delete;

		virtual std::optional<ChunkWork> prepareChunk(ChunkCoord coord) = 0;
	};

	LUNAR_API uint32_t  HeightmapSamplesPerSide(const WorldSettings& settings);
	LUNAR_API glm::vec3 HeightmapNormal(const Heightmap& heightmap, int32_t x, int32_t z, const WorldSettings& settings);
	LUNAR_API void      AppendChunkIndices(Render::MeshData& mesh, const WorldSettings& settings);

	template<typename HeightFunction>
	Heightmap SampleHeightmap(HeightFunction&& height_at, ChunkCoord coord, const WorldSettings& settings)
	{
		const int32_t last_sample = static_cast<int32_t>(settings.chunkQuads) + HEIGHTMAP_BORDER;

		Heightmap heightmap = { .samplesPerSide = HeightmapSamplesPerSide(settings) };
		heightmap.heights.reserve(static_cast<size_t>(heightmap.samplesPerSide) * heightmap.samplesPerSide);

		for (int32_t z = -HEIGHTMAP_BORDER; z <= last_sample; z++)
			for (int32_t x = -HEIGHTMAP_BORDER; x <= last_sample; x++)
				heightmap.heights.push_back(height_at(SampleCoordinate(coord.x, x, settings), SampleCoordinate(coord.z, z, settings)));

		return heightmap;
	}

	template<typename ColorFunction>
	Render::MeshData BuildChunkMesh(const Heightmap& heightmap, ColorFunction&& color_at, ChunkCoord coord, const WorldSettings& settings)
	{
		const int32_t quads             = static_cast<int32_t>(settings.chunkQuads);
		const size_t  vertices_per_side = static_cast<size_t>(settings.chunkQuads) + 1;

		Render::MeshData mesh;
		mesh.vertices.reserve(vertices_per_side * vertices_per_side);

		for (int32_t z = 0; z <= quads; z++)
		{
			for (int32_t x = 0; x <= quads; x++)
			{
				const float     height = heightmap.sample(x, z);
				const glm::vec3 normal = HeightmapNormal(heightmap, x, z, settings);
				const glm::vec3 color  = color_at(SampleCoordinate(coord.x, x, settings), SampleCoordinate(coord.z, z, settings), height, normal);

				mesh.vertices.push_back(Render::Vertex {
					.position = { x * settings.vertexSpacing, height, z * settings.vertexSpacing },
					.uv_x     = static_cast<float>(x) / quads,
					.normal   = normal,
					.uv_y     = static_cast<float>(z) / quads,
					.color    = glm::vec4(color, TERRAIN_VERTEX_ALPHA)
				});
			}
		}

		AppendChunkIndices(mesh, settings);
		return mesh;
	}
}
