#include <trok/gameplay/cargo_shock.hpp>

#include <algorithm>
#include <utility>

namespace trok
{
	namespace
	{
		const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };
	}

	CargoShock::CargoShock(CargoShockSettings settings) noexcept
		: settings(std::move(settings))
	{
	}

	float CargoShock::update(const glm::vec3& velocity, const glm::vec3& up, float delta_time)
	{
		const bool was_overturned = overturned;
		overturned = isOverturned(up);

		float damage = overturned && !was_overturned ? settings.rolloverDamage : 0.f;
		if (previousVelocity.has_value())
		{
			const float change = glm::distance(velocity, *previousVelocity);
			damage += std::max(change - settings.shockThreshold * delta_time, 0.f) * settings.shockDamage;
		}

		previousVelocity = velocity;
		return damage;
	}

	void CargoShock::reset(const glm::vec3& velocity, const glm::vec3& up)
	{
		previousVelocity = velocity;
		overturned       = isOverturned(up);
	}

	bool CargoShock::isOverturned(const glm::vec3& up) const
	{
		return glm::dot(up, WORLD_UP) < settings.rolloverCosine;
	}
}
