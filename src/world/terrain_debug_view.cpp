#include <lunar/world/terrain_debug_view.hpp>
#include <lunar/render/components.hpp>
#include <lunar/core/scene.hpp>

#include <array>
#include <format>
#include <string_view>

namespace lunar::World
{
	namespace
	{
		constexpr std::string_view OVERLAY_NAME_FORMAT = "TerrainDebug {},{}";
		constexpr float            SAMPLE_SIZE         = 0.3f;
		constexpr float            BORDER_HEIGHT       = 0.6f;
		constexpr float            CORNER_WIDTH        = 0.4f;
		constexpr float            CORNER_HEIGHT       = 30.f;
		constexpr float            CUBE_EXTENT         = 2.f;
		constexpr float            HALF                = 0.5f;

		const glm::vec4 SAMPLE_COLOR = { 1.f, 0.85f, 0.1f, 1.f };
		const glm::vec4 BORDER_COLOR = { 0.95f, 0.15f, 0.1f, 1.f };
		const glm::vec3 UP           = { 0.f, 1.f, 0.f };

		void AppendBox(Render::MeshData& mesh, const Render::MeshData& cube, const glm::vec3& centre, const glm::vec3& size, const glm::vec4& color)
		{
			const auto first = static_cast<uint32_t>(mesh.vertices.size());

			for (Render::Vertex vertex : cube.vertices)
			{
				vertex.position = centre + vertex.position * size / CUBE_EXTENT;
				vertex.color    = color;
				mesh.vertices.push_back(vertex);
			}

			for (const uint32_t index : cube.indices)
				mesh.indices.push_back(first + index);
		}

		void AppendStrip(Render::MeshData& mesh, const glm::vec3& from, const glm::vec3& to, const glm::vec4& color)
		{
			const glm::vec3 lift   = UP * BORDER_HEIGHT;
			const glm::vec3 across = glm::normalize(glm::cross(to - from, UP));
			const auto      first  = static_cast<uint32_t>(mesh.vertices.size());

			for (const glm::vec3& corner : { from, to, to + lift, from + lift })
				mesh.vertices.push_back(Render::Vertex { .position = corner, .normal = across, .color = color });

			mesh.indices.insert(mesh.indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3,
			                                          first, first + 2, first + 1, first, first + 3, first + 2 });
		}

		Render::MeshData BuildOverlay(const Heightmap& heightmap, const WorldSettings& settings)
		{
			const Render::MeshData cube  = Render::CreateCubeMeshData();
			const int32_t          quads = static_cast<int32_t>(settings.chunkQuads);

			const auto point = [&](int32_t x, int32_t z) {
				return glm::vec3(static_cast<float>(x) * settings.vertexSpacing, heightmap.sample(x, z), static_cast<float>(z) * settings.vertexSpacing);
			};

			Render::MeshData mesh;
			for (int32_t z = 0; z <= quads; z++)
				for (int32_t x = 0; x <= quads; x++)
					AppendBox(mesh, cube, point(x, z), glm::vec3(SAMPLE_SIZE), SAMPLE_COLOR);

			for (int32_t step = 0; step < quads; step++)
			{
				AppendStrip(mesh, point(step, 0),     point(step + 1, 0),     BORDER_COLOR);
				AppendStrip(mesh, point(step, quads), point(step + 1, quads), BORDER_COLOR);
				AppendStrip(mesh, point(0, step),     point(0, step + 1),     BORDER_COLOR);
				AppendStrip(mesh, point(quads, step), point(quads, step + 1), BORDER_COLOR);
			}

			for (const auto& [x, z] : std::array { std::array { 0, 0 }, std::array { quads, 0 }, std::array { 0, quads }, std::array { quads, quads } })
				AppendBox(mesh, cube, point(x, z) + UP * (CORNER_HEIGHT * HALF), glm::vec3(CORNER_WIDTH, CORNER_HEIGHT, CORNER_WIDTH), BORDER_COLOR);

			return mesh;
		}
	}

	TerrainDebugView::TerrainDebugView(Scene& scene, Render::MeshRegistry& meshes, const TerrainWorld& terrain, const WorldSettings& settings, int32_t radius_chunks) noexcept
		: scene(scene),
		meshes(meshes),
		terrain(terrain),
		settings(settings),
		radiusChunks(radius_chunks)
	{
	}

	TerrainDebugView::~TerrainDebugView() noexcept
	{
		clear();
	}

	void TerrainDebugView::update(const glm::vec3& focus)
	{
		const ChunkCoord centre = ChunkAt(focus, settings);

		std::erase_if(overlays, [&](auto& entry) {
			auto& [coord, overlay] = entry;
			if (IsInsideSquare(coord, centre, radiusChunks) && terrain.getRevision(coord) == overlay.revision)
				return false;

			destroy(overlay);
			return true;
		});

		for (int32_t offset_z = -radiusChunks; offset_z <= radiusChunks; offset_z++)
		{
			for (int32_t offset_x = -radiusChunks; offset_x <= radiusChunks; offset_x++)
			{
				const ChunkCoord coord     = { centre.x + offset_x, centre.z + offset_z };
				const Heightmap* heightmap = terrain.findHeightmap(coord);
				if (heightmap != nullptr && !overlays.contains(coord))
					create(coord, *heightmap);
			}
		}
	}

	void TerrainDebugView::clear()
	{
		for (auto& [coord, overlay] : overlays)
			destroy(overlay);

		overlays.clear();
	}

	void TerrainDebugView::create(ChunkCoord coord, const Heightmap& heightmap)
	{
		Overlay overlay =
		{
			.object   = scene.createGameObject(std::format(OVERLAY_NAME_FORMAT, coord.x, coord.z)),
			.mesh     = meshes.create(BuildOverlay(heightmap, settings)),
			.revision = terrain.getRevision(coord)
		};

		overlay.object->getTransform().position = ChunkOrigin(coord, settings);
		overlay.object->addComponent<MeshRenderer>(overlay.mesh);
		overlays.emplace(coord, std::move(overlay));
	}

	void TerrainDebugView::destroy(Overlay& overlay)
	{
		meshes.destroy(overlay.mesh);
		overlay.object.destroy();
	}
}
