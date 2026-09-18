#include <lunar/render/mesh_registry.hpp>
#include <lunar/debug.hpp>

#include <span>

namespace lunar::Render
{
	namespace
	{
		constexpr float CUBE_HALF_EXTENT = 1.f;

		struct CubeFace
		{
			glm::vec3 normal;
			glm::vec3 tangent;
			glm::vec3 bitangent;
		};

		constexpr CubeFace CUBE_FACES[] =
		{
			{ {  1,  0,  0 }, {  0,  0, -1 }, { 0, 1,  0 } },
			{ { -1,  0,  0 }, {  0,  0,  1 }, { 0, 1,  0 } },
			{ {  0,  1,  0 }, {  1,  0,  0 }, { 0, 0, -1 } },
			{ {  0, -1,  0 }, {  1,  0,  0 }, { 0, 0,  1 } },
			{ {  0,  0,  1 }, {  1,  0,  0 }, { 0, 1,  0 } },
			{ {  0,  0, -1 }, { -1,  0,  0 }, { 0, 1,  0 } }
		};

		constexpr glm::vec2 FACE_CORNERS[] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
		constexpr uint32_t  FACE_INDICES[] = { 0, 1, 2, 2, 3, 0 };
		const glm::vec4     VERTEX_COLOR   = glm::vec4(1.f);
	}

	MeshRegistry::MeshRegistry(RenderDevice& device) noexcept
		: device(device)
	{
	}

	MeshRegistry::~MeshRegistry() noexcept
	{
		meshes.forEach([&](PoolHandle<Mesh>, const Mesh& mesh) {
			releaseBuffers(mesh);
		});
	}

	MeshHandle MeshRegistry::create(const MeshData& data)
	{
		if (data.vertices.empty() || data.indices.empty())
		{
			DEBUG_ERROR("Meshes need at least one vertex and one index");
			return {};
		}

		const std::span<const std::byte> vertex_bytes = std::as_bytes(std::span(data.vertices));
		const std::span<const std::byte> index_bytes  = std::as_bytes(std::span(data.indices));

		const BufferHandle vertex_buffer = device.createBuffer({ vertex_bytes.size(), BufferUsageFlags(BufferUsageFlagBits::eStorage), MemoryLocation::eGpuOnly }, vertex_bytes);
		const BufferHandle index_buffer  = device.createBuffer({ index_bytes.size(),  BufferUsageFlags(BufferUsageFlagBits::eIndex),   MemoryLocation::eGpuOnly }, index_bytes);

		const Mesh mesh =
		{
			.vertexBuffer  = vertex_buffer,
			.indexBuffer   = index_buffer,
			.vertexAddress = device.getBufferAddress(vertex_buffer),
			.indexCount    = static_cast<uint32_t>(data.indices.size()),
			.bounds        = Bounds::FromVertices(data.vertices)
		};

		if (vertex_buffer == BufferHandle {} || index_buffer == BufferHandle {})
		{
			releaseBuffers(mesh);
			return {};
		}

		return ToGpuHandle<MeshTag>(meshes.create(mesh));
	}

	void MeshRegistry::destroy(MeshHandle mesh)
	{
		const PoolHandle<Mesh> stored = FromGpuHandle(meshes, mesh);
		const Mesh*            record = meshes.get(stored);
		if (record == nullptr)
			return;

		releaseBuffers(*record);
		meshes.destroy(stored);
	}

	const Mesh* MeshRegistry::get(MeshHandle mesh)
	{
		return meshes.get(FromGpuHandle(meshes, mesh));
	}

	size_t MeshRegistry::size() const
	{
		return meshes.size();
	}

	void MeshRegistry::releaseBuffers(const Mesh& mesh)
	{
		device.destroyBuffer(mesh.vertexBuffer);
		device.destroyBuffer(mesh.indexBuffer);
	}

	MeshData CreateCubeMeshData()
	{
		MeshData data;

		for (const CubeFace& face : CUBE_FACES)
		{
			const auto first_index = static_cast<uint32_t>(data.vertices.size());

			for (const glm::vec2 corner : FACE_CORNERS)
			{
				const glm::vec3 position = (face.normal
					+ face.tangent   * (corner.x * 2.f - 1.f)
					+ face.bitangent * (corner.y * 2.f - 1.f)) * CUBE_HALF_EXTENT;

				data.vertices.push_back(Vertex { position, corner.x, face.normal, corner.y, VERTEX_COLOR });
			}

			for (const uint32_t face_index : FACE_INDICES)
				data.indices.push_back(first_index + face_index);
		}

		return data;
	}
}
