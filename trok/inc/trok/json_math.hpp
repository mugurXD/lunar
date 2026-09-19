#pragma once
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace trok
{
	nlohmann::json SerializeVec3(const glm::vec3& vector);
	glm::vec3      DeserializeVec3(const nlohmann::json& json);
}
