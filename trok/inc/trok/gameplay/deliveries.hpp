#pragma once
#include <trok/world/region_plan.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace trok
{
	struct DeliverySettings
	{
		double minDistance      = 3000.0;
		double maxDistance      = 12000.0;
		double ratePerKilometre = 100.0;
		double parSpeed         = 15.0;
		double onTimeBonus      = 0.5;
	};

	enum class DeliveryStage
	{
		ePickup,
		eDropoff
	};

	struct Delivery
	{
		Settlement    origin      = {};
		Settlement    destination = {};
		DeliveryStage stage       = DeliveryStage::ePickup;
		int64_t       reward      = 0;
		double        parTime     = 0.0;
		double        elapsed     = 0.0;
	};

	class Deliveries
	{
	public:
		Deliveries(DeliverySettings settings, uint64_t seed) noexcept;

		void                           setTowns(std::vector<Settlement> towns);
		std::optional<int64_t>         update(const glm::dvec2& position, double delta_time);
		const std::optional<Delivery>& getDelivery() const;
		std::optional<glm::dvec2>      getTarget()   const;
		int64_t                        getMoney()    const;

	private:
		std::optional<Delivery> offer(const Settlement& origin);
		int64_t                 complete();

		DeliverySettings        settings;
		std::mt19937_64         random;
		std::vector<Settlement> towns;
		std::optional<Delivery> delivery;
		int64_t                 money = 0;
	};
}
