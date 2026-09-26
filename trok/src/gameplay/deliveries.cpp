#include <trok/gameplay/deliveries.hpp>

#include <lunar/debug.hpp>

#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr double METRES_PER_KILOMETRE = 1000.0;

		bool Contains(const Settlement& town, const glm::dvec2& position)
		{
			return glm::distance(town.center, position) <= town.radius;
		}

		const Settlement* Nearest(std::span<const Settlement> towns, const glm::dvec2& position, const Settlement* excluded = nullptr)
		{
			const Settlement* nearest  = nullptr;
			double            shortest = std::numeric_limits<double>::max();

			for (const Settlement& town : towns)
			{
				const double distance = glm::distance(town.center, position);
				if ((excluded != nullptr && town == *excluded) || distance >= shortest)
					continue;

				nearest  = &town;
				shortest = distance;
			}

			return nearest;
		}
	}

	Deliveries::Deliveries(DeliverySettings settings, uint64_t seed) noexcept
		: settings(std::move(settings)),
		random(seed)
	{
	}

	void Deliveries::setTowns(std::vector<Settlement> replacement)
	{
		towns = std::move(replacement);
	}

	std::optional<int64_t> Deliveries::update(const glm::dvec2& position, double delta_time)
	{
		if (!delivery.has_value())
		{
			const Settlement* nearest = Nearest(towns, position);
			if (nearest != nullptr)
				delivery = offer(*nearest);

			return std::nullopt;
		}

		if (delivery->stage == DeliveryStage::ePickup)
		{
			if (Contains(delivery->origin, position))
				delivery->stage = DeliveryStage::eDropoff;

			return std::nullopt;
		}

		delivery->elapsed += delta_time;
		return Contains(delivery->destination, position) ? std::optional<int64_t>(complete()) : std::nullopt;
	}

	const std::optional<Delivery>& Deliveries::getDelivery() const
	{
		return delivery;
	}

	std::optional<glm::dvec2> Deliveries::getTarget() const
	{
		if (!delivery.has_value())
			return std::nullopt;

		return delivery->stage == DeliveryStage::ePickup ? delivery->origin.center : delivery->destination.center;
	}

	int64_t Deliveries::getMoney() const
	{
		return money;
	}

	std::optional<Delivery> Deliveries::offer(const Settlement& origin)
	{
		std::vector<const Settlement*> candidates;
		for (const Settlement& town : towns)
		{
			const double distance = glm::distance(origin.center, town.center);
			if (!(town == origin) && distance >= settings.minDistance && distance <= settings.maxDistance)
				candidates.push_back(&town);
		}

		const Settlement* destination = candidates.empty()
		                              ? Nearest(towns, origin.center, &origin)
		                              : candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(random)];
		if (destination == nullptr)
			return std::nullopt;

		const double distance = glm::distance(origin.center, destination->center);
		const auto   reward   = static_cast<int64_t>(std::llround(distance / METRES_PER_KILOMETRE * settings.ratePerKilometre));

		DEBUG_LOG("New delivery from ({:.0f}, {:.0f}) to ({:.0f}, {:.0f}): {:.1f} km for {}",
		          origin.center.x, origin.center.y, destination->center.x, destination->center.y, distance / METRES_PER_KILOMETRE, reward);

		return Delivery { .origin = origin, .destination = *destination, .reward = reward, .parTime = distance / settings.parSpeed };
	}

	int64_t Deliveries::complete()
	{
		const bool    on_time = delivery->elapsed <= delivery->parTime;
		const int64_t payout  = delivery->reward + (on_time ? std::llround(static_cast<double>(delivery->reward) * settings.onTimeBonus) : 0);

		money    += payout;
		delivery  = offer(delivery->destination);
		if (delivery.has_value())
			delivery->stage = DeliveryStage::eDropoff;

		return payout;
	}
}
