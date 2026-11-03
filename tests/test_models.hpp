#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

struct TestModel
{
	static constexpr size_t   CORNERS        = 8;
	static constexpr uint32_t FLOAT_TYPE     = 5126;
	static constexpr uint32_t UNSIGNED_TYPE  = 5125;
	static constexpr uint32_t X_BIT          = 1;
	static constexpr uint32_t Y_BIT          = 2;
	static constexpr uint32_t Z_BIT          = 4;
	static constexpr size_t   INDEX_COUNT    = 36;

	static constexpr std::array<uint32_t, INDEX_COUNT> INDICES =
	{
		0, 4, 6, 0, 6, 2,
		1, 3, 7, 1, 7, 5,
		0, 1, 5, 0, 5, 4,
		2, 6, 7, 2, 7, 3,
		0, 2, 3, 0, 3, 1,
		4, 5, 7, 4, 7, 6
	};

	glm::vec3 translation = {};
	glm::vec4 baseColor   = glm::vec4(1.f);
	glm::vec4 vertexColor = glm::vec4(1.f);
	float     halfExtent  = 0.5f;
	bool      normals     = true;
	bool      collider    = true;

	std::vector<glm::vec3> corners() const
	{
		std::vector<glm::vec3> points;
		for (uint32_t corner = 0; corner < CORNERS; corner++)
			points.push_back({ (corner & X_BIT) ? halfExtent : -halfExtent, (corner & Y_BIT) ? halfExtent : -halfExtent, (corner & Z_BIT) ? halfExtent : -halfExtent });

		return points;
	}

	void write(const std::filesystem::path& path) const
	{
		const std::vector<glm::vec3> positions = corners();
		std::vector<glm::vec3>       directions;
		for (const glm::vec3& position : positions)
			directions.push_back(glm::normalize(position));

		const std::vector<glm::vec4> colors(CORNERS, vertexColor);
		std::filesystem::path        binary = path;
		binary.replace_extension(".bin");

		std::ofstream data(binary, std::ios::binary | std::ios::trunc);
		data.write(reinterpret_cast<const char*>(positions.data()), static_cast<std::streamsize>(positions.size() * sizeof(glm::vec3)));
		data.write(reinterpret_cast<const char*>(directions.data()), static_cast<std::streamsize>(directions.size() * sizeof(glm::vec3)));
		data.write(reinterpret_cast<const char*>(colors.data()), static_cast<std::streamsize>(colors.size() * sizeof(glm::vec4)));
		data.write(reinterpret_cast<const char*>(INDICES.data()), static_cast<std::streamsize>(INDICES.size() * sizeof(uint32_t)));

		const size_t vector_bytes = CORNERS * sizeof(glm::vec3);
		const size_t color_bytes  = CORNERS * sizeof(glm::vec4);
		const size_t index_bytes  = INDEX_COUNT * sizeof(uint32_t);

		nlohmann::json attributes = { { "POSITION", 0 }, { "COLOR_0", 2 } };
		if (normals)
			attributes["NORMAL"] = 1;

		nlohmann::json nodes = nlohmann::json::array({ { { "name", "body" }, { "mesh", 0 }, { "translation", { translation.x, translation.y, translation.z } } } });
		if (collider)
			nodes.push_back({ { "name", "body_collider" }, { "mesh", 1 }, { "translation", { translation.x, translation.y, translation.z } } });

		nlohmann::json scene_nodes = nlohmann::json::array({ 0 });
		if (collider)
			scene_nodes.push_back(1);

		const nlohmann::json gltf =
		{
			{ "asset",       { { "version", "2.0" } } },
			{ "scene",       0 },
			{ "scenes",      { { { "nodes", scene_nodes } } } },
			{ "nodes",       nodes },
			{ "meshes",      { { { "primitives", { { { "attributes", attributes }, { "indices", 3 }, { "material", 0 } } } } },
			                   { { "primitives", { { { "attributes", { { "POSITION", 0 } } } } } } } } },
			{ "materials",   { { { "pbrMetallicRoughness", { { "baseColorFactor", { baseColor.r, baseColor.g, baseColor.b, baseColor.a } } } } } } },
			{ "buffers",     { { { "uri", binary.filename().string() }, { "byteLength", vector_bytes * 2 + color_bytes + index_bytes } } } },
			{ "bufferViews", { { { "buffer", 0 }, { "byteOffset", 0 },                              { "byteLength", vector_bytes } },
			                   { { "buffer", 0 }, { "byteOffset", vector_bytes },                   { "byteLength", vector_bytes } },
			                   { { "buffer", 0 }, { "byteOffset", vector_bytes * 2 },               { "byteLength", color_bytes } },
			                   { { "buffer", 0 }, { "byteOffset", vector_bytes * 2 + color_bytes }, { "byteLength", index_bytes } } } },
			{ "accessors",   { { { "bufferView", 0 }, { "componentType", FLOAT_TYPE }, { "count", CORNERS }, { "type", "VEC3" },
			                     { "min", { -halfExtent, -halfExtent, -halfExtent } }, { "max", { halfExtent, halfExtent, halfExtent } } },
			                   { { "bufferView", 1 }, { "componentType", FLOAT_TYPE },    { "count", CORNERS },     { "type", "VEC3" } },
			                   { { "bufferView", 2 }, { "componentType", FLOAT_TYPE },    { "count", CORNERS },     { "type", "VEC4" } },
			                   { { "bufferView", 3 }, { "componentType", UNSIGNED_TYPE }, { "count", INDEX_COUNT }, { "type", "SCALAR" } } } }
		};

		std::ofstream(path, std::ios::trunc) << gltf.dump();
	}
};
