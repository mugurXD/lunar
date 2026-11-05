#pragma once
#include <lunar/api.hpp>
#include <lunar/file/filesystem.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace lunar::Render
{
	struct LUNAR_API ModelData
	{
		MeshData                            mesh      = {};
		std::vector<std::vector<glm::vec3>> colliders = {};
	};

	LUNAR_API std::optional<ModelData> LoadModel(const Fs::Path& path);
}
