#pragma once
#include <lunar/api.hpp>
#include <lunar/core/handle.hpp>
#include <lunar/render/common.hpp>
#include <lunar/render/render_device.hpp>

#include <cstdint>
#include <vector>

namespace lunar::Render
{
	struct LUNAR_API MeshData
	{
		std::vector<Vertex>   vertices = {};
		std::vector<uint32_t> indices  = {};
	};

	struct LUNAR_API Mesh
	{
		BufferHandle vertexBuffer  = {};
		BufferHandle indexBuffer   = {};
		uint64_t     vertexAddress = 0;
		uint32_t     indexCount    = 0;
	};

	class LUNAR_API MeshRegistry
	{
	public:
		MeshRegistry(RenderDevice& device) noexcept;
		~MeshRegistry() noexcept;

		MeshRegistry(const MeshRegistry&)            = delete;
		MeshRegistry& operator=(const MeshRegistry&) = delete;

		MeshHandle  create(const MeshData& data);
		void        destroy(MeshHandle mesh);
		const Mesh* get(MeshHandle mesh);
		size_t      size() const;

	private:
		void releaseBuffers(const Mesh& mesh);

		RenderDevice& device;
		Pool<Mesh>    meshes;
	};

	LUNAR_API MeshData CreateCubeMeshData();
}
