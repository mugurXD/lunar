#include <lunar/world/terrain.hpp>

#include <cmath>
#include <functional>

namespace lunar::World
{
	namespace
	{
		constexpr int32_t HEIGHTMAP_BORDER    = 1;
		constexpr int     COORD_HALF_BITS     = 32;
		constexpr float   CENTRAL_DIFFERENCES = 2.f;
		constexpr float   OPAQUE_ALPHA        = 1.f;
		constexpr size_t  INDICES_PER_QUAD    = 6;

		double WorldCoordinate(int32_t chunk, int32_t sample, const TerrainSettings& settings)
		{
			const int64_t sample_index = static_cast<int64_t>(chunk) * settings.chunkQuads + sample;
			return static_cast<double>(sample_index) * settings.vertexSpacing;
		}

		Heightmap SampleHeightmap(const TerrainGenerator& generator, ChunkCoord coord, const TerrainSettings& settings)
		{
			const int32_t first_sample = -HEIGHTMAP_BORDER;
			const int32_t last_sample  = static_cast<int32_t>(settings.chunkQuads) + HEIGHTMAP_BORDER;

			Heightmap heightmap = { .samplesPerSide = static_cast<uint32_t>(last_sample - first_sample + 1) };
			heightmap.heights.reserve(static_cast<size_t>(heightmap.samplesPerSide) * heightmap.samplesPerSide);

			for (int32_t z = first_sample; z <= last_sample; z++)
				for (int32_t x = first_sample; x <= last_sample; x++)
					heightmap.heights.push_back(generator.sampleHeight(WorldCoordinate(coord.x, x, settings), WorldCoordinate(coord.z, z, settings)));

			return heightmap;
		}

		glm::vec3 NormalAt(const Heightmap& heightmap, int32_t x, int32_t z, float spacing)
		{
			const float slope_x = heightmap.sample(x + 1, z) - heightmap.sample(x - 1, z);
			const float slope_z = heightmap.sample(x, z + 1) - heightmap.sample(x, z - 1);

			return glm::normalize(glm::vec3(-slope_x, CENTRAL_DIFFERENCES * spacing, -slope_z));
		}

		Render::MeshData BuildMesh(const TerrainGenerator& generator, const Heightmap& heightmap, ChunkCoord coord, const TerrainSettings& settings)
		{
			const int32_t  quads             = static_cast<int32_t>(settings.chunkQuads);
			const uint32_t vertices_per_side = settings.chunkQuads + 1;

			Render::MeshData mesh;
			mesh.vertices.reserve(static_cast<size_t>(vertices_per_side) * vertices_per_side);
			mesh.indices.reserve(static_cast<size_t>(settings.chunkQuads) * settings.chunkQuads * INDICES_PER_QUAD);

			for (int32_t z = 0; z <= quads; z++)
			{
				for (int32_t x = 0; x <= quads; x++)
				{
					const float     height = heightmap.sample(x, z);
					const glm::vec3 normal = NormalAt(heightmap, x, z, settings.vertexSpacing);
					const glm::vec3 color  = generator.sampleColor(WorldCoordinate(coord.x, x, settings), WorldCoordinate(coord.z, z, settings), height, normal);

					mesh.vertices.push_back(Render::Vertex {
						.position = { x * settings.vertexSpacing, height, z * settings.vertexSpacing },
						.uv_x     = static_cast<float>(x) / quads,
						.normal   = normal,
						.uv_y     = static_cast<float>(z) / quads,
						.color    = glm::vec4(color, OPAQUE_ALPHA)
					});
				}
			}

			for (uint32_t z = 0; z < settings.chunkQuads; z++)
			{
				for (uint32_t x = 0; x < settings.chunkQuads; x++)
				{
					const uint32_t top_left     = z * vertices_per_side + x;
					const uint32_t top_right    = top_left + 1;
					const uint32_t bottom_left  = top_left + vertices_per_side;
					const uint32_t bottom_right = bottom_left + 1;

					mesh.indices.insert(mesh.indices.end(), { top_left, bottom_left, top_right, top_right, bottom_left, bottom_right });
				}
			}

			return mesh;
		}
	}

	size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const
	{
		const uint64_t packed = (static_cast<uint64_t>(static_cast<uint32_t>(coord.x)) << COORD_HALF_BITS) | static_cast<uint32_t>(coord.z);
		return std::hash<uint64_t> {}(packed);
	}

	float TerrainSettings::getChunkSize() const
	{
		return static_cast<float>(chunkQuads) * vertexSpacing;
	}

	float Heightmap::sample(int32_t x, int32_t z) const
	{
		const size_t row    = static_cast<size_t>(z + HEIGHTMAP_BORDER);
		const size_t column = static_cast<size_t>(x + HEIGHTMAP_BORDER);
		return heights[row * samplesPerSide + column];
	}

	ChunkCoord ChunkAt(const glm::vec3& position, const TerrainSettings& settings)
	{
		const float chunk_size = settings.getChunkSize();

		return ChunkCoord
		{
			.x = static_cast<int32_t>(std::floor(position.x / chunk_size)),
			.z = static_cast<int32_t>(std::floor(position.z / chunk_size))
		};
	}

	glm::vec3 ChunkOrigin(ChunkCoord coord, const TerrainSettings& settings)
	{
		return glm::vec3(static_cast<float>(WorldCoordinate(coord.x, 0, settings)), 0.f, static_cast<float>(WorldCoordinate(coord.z, 0, settings)));
	}

	ChunkData GenerateChunk(const TerrainGenerator& generator, ChunkCoord coord, const TerrainSettings& settings)
	{
		Heightmap        heightmap = SampleHeightmap(generator, coord, settings);
		Render::MeshData mesh      = BuildMesh(generator, heightmap, coord, settings);

		return ChunkData { std::move(heightmap), std::move(mesh) };
	}
}
