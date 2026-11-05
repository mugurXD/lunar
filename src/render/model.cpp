#include <lunar/render/model.hpp>
#include <lunar/debug.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <span>
#include <string_view>

namespace lunar::Render
{
	namespace
	{
		constexpr std::string_view COLLIDER_SUFFIX    = "_collider";
		constexpr std::string_view POSITION_ATTRIBUTE = "POSITION";
		constexpr std::string_view NORMAL_ATTRIBUTE   = "NORMAL";
		constexpr std::string_view COLOR_ATTRIBUTE    = "COLOR_0";
		constexpr glm::length_t    MATRIX_SIZE        = 4;
		constexpr size_t           TRIANGLE_CORNERS   = 3;
		constexpr size_t           DEFAULT_SCENE      = 0;

		glm::mat4 ToGlm(const fastgltf::math::fmat4x4& matrix)
		{
			glm::mat4 converted(1.f);
			for (glm::length_t column = 0; column < MATRIX_SIZE; column++)
				for (glm::length_t row = 0; row < MATRIX_SIZE; row++)
					converted[column][row] = matrix[column][row];

			return converted;
		}

		const fastgltf::Accessor* FindAccessor(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, std::string_view name)
		{
			const auto attribute = primitive.findAttribute(name);
			return attribute == primitive.attributes.end() ? nullptr : &asset.accessors[attribute->accessorIndex];
		}

		glm::vec4 BaseColor(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive)
		{
			if (!primitive.materialIndex.has_value())
				return glm::vec4(1.f);

			const fastgltf::math::nvec4& factor = asset.materials[*primitive.materialIndex].pbrData.baseColorFactor;
			return glm::vec4(factor.x(), factor.y(), factor.z(), factor.w());
		}

		void TintWithColors(const fastgltf::Asset& asset, const fastgltf::Accessor& colors, std::span<Vertex> vertices)
		{
			const auto tint = [&](const glm::vec4& color, size_t index) { vertices[index].color *= color; };

			if (colors.type == fastgltf::AccessorType::Vec3)
				fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, colors, [&](const glm::vec3& color, size_t index) { tint(glm::vec4(color, 1.f), index); });
			else
				fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colors, tint);
		}

		void AccumulateFaceNormals(MeshData& mesh, uint32_t first_vertex, size_t first_index)
		{
			for (size_t corner = first_index; corner + TRIANGLE_CORNERS <= mesh.indices.size(); corner += TRIANGLE_CORNERS)
			{
				Vertex&         first  = mesh.vertices[mesh.indices[corner]];
				Vertex&         second = mesh.vertices[mesh.indices[corner + 1]];
				Vertex&         third  = mesh.vertices[mesh.indices[corner + 2]];
				const glm::vec3 facing = glm::cross(second.position - first.position, third.position - first.position);

				first.normal  += facing;
				second.normal += facing;
				third.normal  += facing;
			}

			for (Vertex& vertex : std::span(mesh.vertices).subspan(first_vertex))
				if (glm::length(vertex.normal) > 0.f)
					vertex.normal = glm::normalize(vertex.normal);
		}

		void AppendIndices(MeshData& mesh, const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, uint32_t first_vertex)
		{
			if (!primitive.indicesAccessor.has_value())
			{
				for (uint32_t vertex = first_vertex; vertex < mesh.vertices.size(); vertex++)
					mesh.indices.push_back(vertex);

				return;
			}

			fastgltf::iterateAccessor<uint32_t>(asset, asset.accessors[*primitive.indicesAccessor], [&](uint32_t index) {
				mesh.indices.push_back(first_vertex + index);
			});
		}

		void AppendPrimitive(MeshData& mesh, const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, const glm::mat4& transform)
		{
			const fastgltf::Accessor* positions = FindAccessor(asset, primitive, POSITION_ATTRIBUTE);
			if (positions == nullptr || primitive.type != fastgltf::PrimitiveType::Triangles)
			{
				DEBUG_WARN("Skipped a model primitive that is not a triangle list with positions");
				return;
			}

			const auto      first_vertex     = static_cast<uint32_t>(mesh.vertices.size());
			const size_t    first_index      = mesh.indices.size();
			const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(transform)));
			const glm::vec4 base_color       = BaseColor(asset, primitive);

			fastgltf::iterateAccessor<glm::vec3>(asset, *positions, [&](const glm::vec3& position) {
				mesh.vertices.push_back(Vertex { .position = glm::vec3(transform * glm::vec4(position, 1.f)), .color = base_color });
			});

			const std::span<Vertex> added = std::span(mesh.vertices).subspan(first_vertex);
			if (const fastgltf::Accessor* colors = FindAccessor(asset, primitive, COLOR_ATTRIBUTE))
				TintWithColors(asset, *colors, added);

			AppendIndices(mesh, asset, primitive, first_vertex);

			const fastgltf::Accessor* normals = FindAccessor(asset, primitive, NORMAL_ATTRIBUTE);
			if (normals == nullptr)
			{
				AccumulateFaceNormals(mesh, first_vertex, first_index);
				return;
			}

			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, *normals, [&](const glm::vec3& normal, size_t index) {
				added[index].normal = glm::normalize(normal_transform * normal);
			});
		}

		std::vector<glm::vec3> CollectPoints(const fastgltf::Asset& asset, const fastgltf::Mesh& mesh, const glm::mat4& transform)
		{
			std::vector<glm::vec3> points;
			for (const fastgltf::Primitive& primitive : mesh.primitives)
				if (const fastgltf::Accessor* positions = FindAccessor(asset, primitive, POSITION_ATTRIBUTE))
					fastgltf::iterateAccessor<glm::vec3>(asset, *positions, [&](const glm::vec3& position) {
						points.push_back(glm::vec3(transform * glm::vec4(position, 1.f)));
					});

			return points;
		}
	}

	std::optional<ModelData> LoadModel(const Fs::Path& path)
	{
		fastgltf::Expected<fastgltf::GltfDataBuffer> data = fastgltf::GltfDataBuffer::FromPath(path);
		if (data.error() != fastgltf::Error::None)
		{
			DEBUG_ERROR("Could not read model '{}': {}", path.string(), fastgltf::getErrorMessage(data.error()));
			return std::nullopt;
		}

		fastgltf::Parser                    parser;
		fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers);
		if (asset.error() != fastgltf::Error::None || asset->scenes.empty())
		{
			DEBUG_ERROR("Could not parse model '{}': {}", path.string(), fastgltf::getErrorMessage(asset.error()));
			return std::nullopt;
		}

		ModelData model;
		fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value_or(DEFAULT_SCENE), fastgltf::math::fmat4x4(), [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
			if (!node.meshIndex.has_value())
				return;

			const fastgltf::Mesh& mesh      = asset->meshes[*node.meshIndex];
			const glm::mat4       transform = ToGlm(matrix);

			if (std::string_view(node.name).ends_with(COLLIDER_SUFFIX))
			{
				model.colliders.push_back(CollectPoints(asset.get(), mesh, transform));
				return;
			}

			for (const fastgltf::Primitive& primitive : mesh.primitives)
				AppendPrimitive(model.mesh, asset.get(), primitive, transform);
		});

		return model;
	}
}
