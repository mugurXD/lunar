#include <trok/gameplay/deliveries.hpp>
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
	constexpr uint64_t SEED        = 7;
	constexpr double   TOWN_RADIUS = 200.0;
	constexpr double   NEAR_TOWN   = 5000.0;
	constexpr double   FAR_TOWN    = 20000.0;
	constexpr double   FRAME       = 1.0 / 60.0;
	constexpr double   LONG_WAIT   = 10000.0;

	const glm::dvec2 OUTSIDE_A = { 1000.0, 0.0 };

	trok::Settlement Town(uint32_t id, double x)
	{
		return { .id = id, .type = "test:town", .center = { x, 0.0 }, .radius = TOWN_RADIUS };
	}

	const trok::Settlement A = Town(1, 0.0);
	const trok::Settlement B = Town(2, NEAR_TOWN);
	const trok::Settlement C = Town(3, FAR_TOWN);

	trok::Deliveries WithTowns(std::vector<trok::Settlement> towns)
	{
		trok::Deliveries deliveries({}, SEED);
		deliveries.setTowns(std::move(towns));
		return deliveries;
	}
}

TEST(Deliveries, WithoutTownsThereIsNothingToDeliver)
{
	trok::Deliveries deliveries({}, SEED);

	EXPECT_FALSE(deliveries.update(OUTSIDE_A, FRAME).has_value());
	EXPECT_FALSE(deliveries.getDelivery().has_value());
	EXPECT_FALSE(deliveries.getTarget().has_value());
}

TEST(Deliveries, TheFirstJobIsPickedUpAtTheNearestTown)
{
	trok::Deliveries deliveries = WithTowns({ A, B, C });
	deliveries.update(OUTSIDE_A, FRAME);

	ASSERT_TRUE(deliveries.getDelivery().has_value());
	EXPECT_EQ(deliveries.getDelivery()->origin, A);
	EXPECT_EQ(deliveries.getDelivery()->destination, B) << "only B lies within the delivery distance range";
	EXPECT_EQ(deliveries.getDelivery()->stage, trok::DeliveryStage::ePickup);
	EXPECT_EQ(deliveries.getTarget(), A.center);
}

TEST(Deliveries, FastDeliveriesPayTheRewardAndTheBonus)
{
	const trok::DeliverySettings settings   = {};
	trok::Deliveries             deliveries = WithTowns({ A, B, C });

	deliveries.update(OUTSIDE_A, FRAME);
	deliveries.update(A.center, FRAME);
	EXPECT_EQ(deliveries.getTarget(), B.center) << "the beacon moves to the destination once the cargo is picked up";

	const int64_t                reward = deliveries.getDelivery()->reward;
	const std::optional<int64_t> payout = deliveries.update(B.center, FRAME);

	ASSERT_TRUE(payout.has_value());
	EXPECT_EQ(reward, std::llround(NEAR_TOWN / 1000.0 * settings.ratePerKilometre));
	EXPECT_EQ(*payout, reward + std::llround(static_cast<double>(reward) * settings.onTimeBonus));
	EXPECT_EQ(deliveries.getMoney(), *payout);
}

TEST(Deliveries, LateDeliveriesEarnNoBonus)
{
	trok::Deliveries deliveries = WithTowns({ A, B, C });

	deliveries.update(OUTSIDE_A, FRAME);
	deliveries.update(A.center, FRAME);
	deliveries.update(OUTSIDE_A, LONG_WAIT);

	const int64_t reward = deliveries.getDelivery()->reward;
	EXPECT_EQ(deliveries.update(B.center, FRAME), reward);
}

TEST(Deliveries, TheNextJobStartsWhereTheLastOneEnded)
{
	trok::Deliveries deliveries = WithTowns({ A, B, C });

	deliveries.update(OUTSIDE_A, FRAME);
	deliveries.update(A.center, FRAME);
	deliveries.update(B.center, FRAME);

	ASSERT_TRUE(deliveries.getDelivery().has_value());
	EXPECT_EQ(deliveries.getDelivery()->origin, B);
	EXPECT_EQ(deliveries.getDelivery()->stage, trok::DeliveryStage::eDropoff) << "the truck is already loaded in the town it delivered to";
}

TEST(Deliveries, WithoutTownsInRangeTheNearestOtherTownIsUsed)
{
	trok::Deliveries deliveries = WithTowns({ A, C });
	deliveries.update(OUTSIDE_A, FRAME);

	ASSERT_TRUE(deliveries.getDelivery().has_value());
	EXPECT_EQ(deliveries.getDelivery()->destination, C);
}
