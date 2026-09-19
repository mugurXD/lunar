#include <trok/json_math.hpp>

namespace trok
{
	nlohmann::json SerializeVec3(const glm::vec3& vector)
	{
		return nlohmann::json { vector.x, vector.y, vector.z };
	}

	glm::vec3 DeserializeVec3(const nlohmann::json& json)
	{
		return glm::vec3(json.at(0).get<float>(), json.at(1).get<float>(), json.at(2).get<float>());
	}
}
