#include <lunar/render.hpp>
#include <lunar/debug.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

namespace lunar::Render
{
	GpuMeshBuilder& GpuMeshBuilder::useRenderContext(RenderContext_T* context)
	{
		this->context = context;
		return *this;
	}

	//GpuMeshBuilder& GpuMeshBuilder::defaultVertexArray()
	//{
	//	this->vertexArray = context->createVertexArray();
	//	return *this;
	//}

	GpuMeshBuilder& GpuMeshBuilder::fromVertexArray(const std::span<const Vertex>& vertices)
	{
		this->vertexBuffer = context->createBuffer(
			GpuBufferType::eVertex,
			GpuBufferUsageFlagBits::eStatic,
			vertices.size() * sizeof(Render::Vertex),
			(void*)vertices.data()
		);
		return *this;
	}

	GpuMeshBuilder& GpuMeshBuilder::fromIndexArray(const std::span<const uint32_t>& indices)
	{
		this->indexBuffer = context->createBuffer(
			GpuBufferType::eIndex,
			GpuBufferUsageFlagBits::eStatic,
			indices.size() * sizeof(uint32_t),
			(void*)indices.data()
		);

		return *this;
	}

	struct MaterialTextureData
	{
		int startX;
		int startY;
		int width;
		int height;
	};

	struct TextureAtlasInfo
	{
		int                                    totalWidth  = 0;
		int                                    totalHeight = 1;
		int                                    textures    = 0;
		std::unique_ptr<MaterialTextureData[]> materials   = nullptr;
		GpuTexture                             handle      = nullptr;
	};

	inline void CreateTextureAtlas
	(
		RenderContext_T*  context,
		fastgltf::Asset&  asset,
		TextureAtlasInfo& output
	)
	{
		auto  colors    = std::vector<uint32_t>();
		auto& materials = asset.materials;

		for (fastgltf::Material& material : materials)
		{
			if (material.pbrData.baseColorTexture.has_value())
				continue;
			else
				output.textures++;
		}

		output.materials = std::make_unique<MaterialTextureData[]>(output.textures);

		size_t i = 0;
		for(fastgltf::Material& material : materials)
		{
			if (material.pbrData.baseColorTexture.has_value())
				continue;

			auto color = material.pbrData.baseColorFactor;
			
			uint32_t value = 0;
			value |= static_cast<uint32_t>(glm::round(color.x() * 255.f));
			value |= static_cast<uint32_t>(glm::round(color.y() * 255.f)) << 8;;
			value |= static_cast<uint32_t>(glm::round(color.z() * 255.f)) << 16;
			value |= static_cast<uint32_t>(glm::round(color.w() * 255.f)) << 24;
			colors.push_back(value);

			auto& data = output.materials[i];
			data.width  = 1;
			data.height = 1;
			data.startX = output.totalWidth;
			data.startY = 0;

			output.totalWidth  += 1;
			i++;
		}

		output.handle = context->createTexture
		(
			output.totalWidth,
			output.totalHeight,
			colors.data(),
			TextureFormat::eRGBA,
			TextureDataFormat::eUnsignedByte,
			TextureFormat::eRGBA,
			TextureType::e2D,
			TextureFiltering::eNearest,
			TextureFiltering::eNearest,
			TextureWrapping::eRepeat
		);
	}

	struct MappedMaterial
	{
		glm::vec2  atlasBegin = { 0, 0 };
		glm::vec2  atlasEnd   = { 0, 0 };
		float      metallic   = 0.1f;
		float      roughness  = 0.1f;
		float      ao         = 0.1f;
		float      padding    = 0.69f;
	};

	inline void MapMaterialData
	(
		fastgltf::Asset&  asset,
		TextureAtlasInfo& atlasInfo,
		MappedMaterial*   output,
		size_t&           i
	)
	{
		auto& materials = asset.materials;
		
		i = 0;
		for (fastgltf::Material& material : materials)
		{
			if (material.pbrData.baseColorTexture.has_value())
				continue;

			output[i].metallic   = glm::max(material.pbrData.metallicFactor, 0.1f);
			output[i].roughness  = glm::max(material.pbrData.roughnessFactor, 0.1f);
			output[i].ao         = glm::clamp(material.ior, 1.f, 4.f) / 4.f;
			output[i].atlasBegin = glm::vec2
			{
				(float)atlasInfo.materials[i].startX / (float)atlasInfo.totalWidth,
				(float)atlasInfo.materials[i].startY / (float)atlasInfo.totalHeight
			};
			output[i].atlasEnd   = glm::vec2
			{
				(float)(atlasInfo.materials[i].startX + atlasInfo.materials[i].width) / (float)atlasInfo.totalWidth,
				(float)(atlasInfo.materials[i].startY + atlasInfo.materials[i].height) / (float)atlasInfo.totalHeight
			};
			i++;
		}
	}

	void GpuMeshBuilder::fromGltfFile(const Fs::Path& path)
	{
		auto options = fastgltf::Options::LoadGLBBuffers | 
						fastgltf::Options::LoadExternalBuffers | 
						fastgltf::Options::LoadExternalImages;

		auto parser  = fastgltf::Parser {};
		auto data    = fastgltf::GltfDataBuffer::FromPath(path);
		if (data.error() != fastgltf::Error::None)
		{
			DEBUG_ERROR("Couldn't open GLTF file at '{}'.", path.generic_string());
			return;
		}

		auto asset = parser.loadGltf(data.get(), path.parent_path(), options);
		if (asset.error() != fastgltf::Error::None)
		{
			DEBUG_ERROR("Couldn't parse GLTF file at '{}'.", path.generic_string());
			return;
		}
		// todo; error

		auto& meshes = asset->meshes;
		
		if (meshes.size() <= 0)
		{
			DEBUG_WARN("File '{}' does not contain any meshes.", path.generic_string());
			return;
		}

		TextureAtlasInfo atlas_info     = {};
		MappedMaterial   materials[20]  = {};
		size_t           material_count = 0;

		CreateTextureAtlas(context, asset.get(), atlas_info);
		MapMaterialData(asset.get(), atlas_info, materials, material_count);

		auto& mesh = asset->meshes[0];

		auto vertices         = std::vector<Vertex>();
		auto indices          = std::vector<uint32_t>();
		auto material_indices = std::vector<int>();
		vertices.reserve(mesh.primitives.size() * 5); 
		indices.reserve(mesh.primitives.size() * 5);
		material_indices.reserve(mesh.primitives.size());

		for (auto&& p : mesh.primitives)
		{
			size_t initial_idx    = vertices.size();
			auto&  index_accessor = asset->accessors[p.indicesAccessor.value()];
			
			int    material_idx   = p.materialIndex.value_or(0);

			fastgltf::iterateAccessor<uint32_t>(asset.get(), index_accessor, [&](uint32_t idx) {
				indices.emplace_back(idx + initial_idx);

				if(indices.size() % 3 == 0)
					material_indices.emplace_back(material_idx);
			});

			auto& pos_accessor = asset->accessors[p.findAttribute("POSITION")->accessorIndex];
			vertices.resize(vertices.size() + pos_accessor.count);

			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), pos_accessor, [&](glm::vec3 v, size_t idx) {
				auto& vertex = vertices[initial_idx + idx];
				vertex.position   = v;
				vertex.normal     = { 0.f, 0.f, 0.f };
				vertex.uv_x       = 0.f;
				vertex.uv_y       = 0.f;
				vertex.color      = glm::vec4{ 1.f };
			});

			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					asset.get(),
					asset->accessors[normals->accessorIndex],
					[&](glm::vec3 v, size_t idx) {
						vertices[initial_idx + idx].normal = v;
					}
				);
			}

			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec2>(
					asset.get(),
					asset->accessors[uv->accessorIndex],
					[&](glm::vec2 v, size_t idx) {
						vertices[initial_idx + idx].uv_x = v.x;
						vertices[initial_idx + idx].uv_y = v.y;
					}
				);
			}

			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					asset.get(),
					asset->accessors[colors->accessorIndex],
					[&](glm::vec4 v, size_t idx) {
						vertices[initial_idx + idx].color = v;
					}
				);
			}
		}

		this->vertexBuffer = context->createBuffer(
			GpuBufferType::eVertex,
			GpuBufferUsageFlagBits::eStatic,
			vertices.size() * sizeof(Vertex),
			vertices.data()
		);

		this->indexBuffer = context->createBuffer(
			GpuBufferType::eIndex,
			GpuBufferUsageFlagBits::eStatic,
			indices.size() * sizeof(uint32_t),
			indices.data()
		);

		size_t buf_size    = sizeof(MappedMaterial) * 20 + (material_indices.size() * sizeof(int));
		auto   data_buffer = std::make_unique<uint8_t[]>(buf_size);
		std::memcpy(data_buffer.get(), materials, sizeof(MappedMaterial) * 20);
		std::memcpy(data_buffer.get() + sizeof(MappedMaterial) * 20, material_indices.data(), material_indices.size() * sizeof(int));

		this->materialsBuffer = context->createBuffer(
			GpuBufferType::eShaderStorage,
			GpuBufferUsageFlagBits::eStatic,
			buf_size,
			data_buffer.get()
		);

		this->materialsAtlas = atlas_info.handle;
	}

	GpuMeshBuilder& GpuMeshBuilder::fromMeshFile(const Fs::Path& path)
	{
		auto extension = path.extension().generic_string();
		if (extension.compare(".gltf") == 0 || extension.compare(".glb") == 0)
			fromGltfFile(path);

		return *this;
	}

	GpuMesh GpuMeshBuilder::build()
	{
		return context->createMesh(vertexBuffer, indexBuffer, MeshTopology::eTriangles, materialsBuffer, materialsAtlas);
	}

	GpuMesh RenderContext_T::getMesh(MeshPrimitive primitive)
	{
		switch (primitive)
		{
		case MeshPrimitive::eCube: return cubeMesh;
		case MeshPrimitive::eQuad: return quadMesh;
		default:                   return nullptr;
		}
	}
}