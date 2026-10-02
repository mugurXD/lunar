#include <trok/gameplay/deliveries.hpp>
#include <trok/world/road.hpp>

#include <lunar/debug.hpp>

#include <algorithm>
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

		std::optional<size_t> TownAt(std::span<const Settlement> towns, const glm::vec3& point)
		{
			for (size_t town = 0; town < towns.size(); town++)
				if (Contains(towns[town], { point.x, point.z }))
					return town;

			return std::nullopt;
		}

		std::vector<std::optional<size_t>> ConnectedGroups(std::span<const Settlement> towns, const RoadNetwork* network)
		{
			std::vector<std::optional<size_t>> groups(towns.size());
			if (network == nullptr)
				return groups;

			std::vector<std::vector<size_t>> neighbours(towns.size());
			for (size_t road = 0; road < network->getRoadCount(); road++)
			{
				const std::span<const glm::vec3> points = network->getRoad(road);
				const std::optional<size_t>      from   = TownAt(towns, points.front());
				const std::optional<size_t>      to     = TownAt(towns, points.back());
				if (!from.has_value() || !to.has_value() || from == to)
					continue;

				neighbours[*from].push_back(*to);
				neighbours[*to].push_back(*from);
			}

			size_t next_group = 0;
			for (size_t start = 0; start < towns.size(); start++)
			{
				if (groups[start].has_value() || neighbours[start].empty())
					continue;

				std::vector<size_t> pending = { start };
				groups[start]               = next_group;
				while (!pending.empty())
				{
					const size_t town = pending.back();
					pending.pop_back();

					for (const size_t neighbour : neighbours[town])
					{
						if (groups[neighbour].has_value())
							continue;

						groups[neighbour] = next_group;
						pending.push_back(neighbour);
					}
				}

				next_group++;
			}

			return groups;
		}
	}

	Deliveries::Deliveries(DeliverySettings settings, uint64_t seed) noexcept
		: settings(std::move(settings)),
		random(seed)
	{
	}

	void Deliveries::setTowns(std::vector<Settlement> replacement)
	{
		towns      = std::move(replacement);
		townGroups = ConnectedGroups(towns, network.get());
	}

	void Deliveries::setRoads(std::shared_ptr<const RoadNetwork> replacement)
	{
		network    = std::move(replacement);
		townGroups = ConnectedGroups(towns, network.get());
	}

	std::optional<int64_t> Deliveries::update(const glm::dvec2& position, double delta_time)
	{
		if (!delivery.has_value())
		{
			const Settlement* nearest = nearestConnected(position, std::nullopt);
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

	void Deliveries::charge(int64_t amount)
	{
		money = std::max<int64_t>(money - amount, 0);
	}

	void Deliveries::damageCargo(float amount)
	{
		if (delivery.has_value() && delivery->stage == DeliveryStage::eDropoff)
			delivery->condition = std::max(delivery->condition - amount, 0.f);
	}

	std::optional<Delivery> Deliveries::offer(const Settlement& origin)
	{
		const std::optional<size_t> group = groupOf(origin);
		if (!group.has_value())
			return std::nullopt;

		std::vector<const Settlement*> candidates;
		for (size_t town = 0; town < towns.size(); town++)
		{
			const double distance = glm::distance(origin.center, towns[town].center);
			if (townGroups[town] == group && !(towns[town] == origin) && distance >= settings.minDistance && distance <= settings.maxDistance)
				candidates.push_back(&towns[town]);
		}

		const Settlement* destination = candidates.empty()
		                              ? nearestConnected(origin.center, group, &origin)
		                              : candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(random)];
		if (destination == nullptr)
			return std::nullopt;

		const double distance = glm::distance(origin.center, destination->center);
		const auto   reward   = static_cast<int64_t>(std::llround(distance / METRES_PER_KILOMETRE * settings.ratePerKilometre));

		DEBUG_LOG("New delivery from ({:.0f}, {:.0f}) to ({:.0f}, {:.0f}): {:.1f} km for {}",
		          origin.center.x, origin.center.y, destination->center.x, destination->center.y, distance / METRES_PER_KILOMETRE, reward);

		return Delivery { .origin = origin, .destination = *destination, .reward = reward, .parTime = distance / settings.parSpeed };
	}

	const Settlement* Deliveries::nearestConnected(const glm::dvec2& position, std::optional<size_t> group, const Settlement* excluded) const
	{
		const Settlement* nearest  = nullptr;
		double            shortest = std::numeric_limits<double>::max();

		for (size_t town = 0; town < towns.size(); town++)
		{
			const double distance  = glm::distance(towns[town].center, position);
			const bool   reachable = townGroups[town].has_value() && (!group.has_value() || townGroups[town] == group);
			if (!reachable || (excluded != nullptr && towns[town] == *excluded) || distance >= shortest)
				continue;

			nearest  = &towns[town];
			shortest = distance;
		}

		return nearest;
	}

	std::optional<size_t> Deliveries::groupOf(const Settlement& town) const
	{
		const auto found = std::ranges::find(towns, town);
		return found == towns.end() ? std::nullopt : townGroups[static_cast<size_t>(found - towns.begin())];
	}

	int64_t Deliveries::complete()
	{
		const bool    on_time = delivery->elapsed <= delivery->parTime;
		const double  earned  = static_cast<double>(delivery->reward) * (on_time ? 1.0 + settings.onTimeBonus : 1.0);
		const int64_t payout  = std::llround(earned * delivery->condition);

		money    += payout;
		delivery  = offer(delivery->destination);
		if (delivery.has_value())
			delivery->stage = DeliveryStage::eDropoff;

		return payout;
	}
}
