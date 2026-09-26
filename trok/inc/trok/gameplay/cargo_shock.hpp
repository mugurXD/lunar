#pragma once
#include <glm/glm.hpp>

#include <optional>

namespace trok
{
	struct CargoShockSettings
	{
		float shockThreshold = 100.f;
		float shockDamage    = 0.002f;
		float rolloverDamage = 0.05f;
		float rolloverCosine = 0.5f;
	};

	class CargoShock
	{
	public:
		explicit CargoShock(CargoShockSettings settings = {}) noexcept;

		float update(const glm::vec3& velocity, const glm::vec3& up, float delta_time);
		void  reset(const glm::vec3& velocity, const glm::vec3& up);

	private:
		bool isOverturned(const glm::vec3& up) const;

		CargoShockSettings       settings;
		std::optional<glm::vec3> previousVelocity = std::nullopt;
		bool                     overturned       = false;
	};
}
