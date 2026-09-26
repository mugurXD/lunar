#include <trok/gameplay/cargo_shock.hpp>
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

TEST(Deliveries, ChargesNeverTakeMoneyBelowZero)
{
	constexpr int64_t FEE = 100;

	trok::Deliveries deliveries = WithTowns({ A, B, C });
	deliveries.update(OUTSIDE_A, FRAME);
	deliveries.update(A.center, FRAME);
	deliveries.update(B.center, FRAME);

	const int64_t earned = deliveries.getMoney();
	deliveries.charge(FEE);
	EXPECT_EQ(deliveries.getMoney(), earned - FEE);

	deliveries.charge(earned);
	EXPECT_EQ(deliveries.getMoney(), 0);
}

namespace
{
	constexpr float STEP          = 1.f / 60.f;
	constexpr float CRUISE_SPEED  = 20.f;
	constexpr float GENTLE_DECEL  = 8.f;
	constexpr float FREE_FALL     = 18.f;
	constexpr int   SAMPLE_STEPS  = 120;
	constexpr float HALF_DAMAGE   = 0.5f;
	constexpr float DAMAGE_MARGIN = 0.001f;

	const glm::vec3 UPRIGHT     = { 0.f, 1.f, 0.f };
	const glm::vec3 ON_ITS_SIDE = { 1.f, 0.f, 0.f };
	const glm::vec3 FORWARD     = { 0.f, 0.f, -1.f };
	const glm::vec3 DOWN        = { 0.f, -1.f, 0.f };

	trok::Deliveries Loaded()
	{
		trok::Deliveries deliveries = WithTowns({ A, B, C });
		deliveries.update(OUTSIDE_A, FRAME);
		deliveries.update(A.center, FRAME);
		return deliveries;
	}
}

TEST(CargoShock, BrakingAndFallingDoNotDamageTheCargo)
{
	trok::CargoShock braking;
	trok::CargoShock falling;
	float            damage = 0.f;

	for (int step = 0; step < SAMPLE_STEPS; step++)
	{
		damage += braking.update(FORWARD * std::max(CRUISE_SPEED - GENTLE_DECEL * STEP * step, 0.f), UPRIGHT, STEP);
		damage += falling.update(DOWN * FREE_FALL * STEP * static_cast<float>(step), UPRIGHT, STEP);
	}

	EXPECT_EQ(damage, 0.f);
}

TEST(CargoShock, SuddenStopsDamageInProportionToTheShock)
{
	const trok::CargoShockSettings settings = {};
	trok::CargoShock               shock(settings);

	shock.update(FORWARD * CRUISE_SPEED, UPRIGHT, STEP);
	const float damage = shock.update({}, UPRIGHT, STEP);

	EXPECT_NEAR(damage, (CRUISE_SPEED - settings.shockThreshold * STEP) * settings.shockDamage, DAMAGE_MARGIN);
}

TEST(CargoShock, RolloversCountOncePerRollover)
{
	const trok::CargoShockSettings settings = {};
	trok::CargoShock               shock(settings);

	shock.update({}, UPRIGHT, STEP);
	EXPECT_EQ(shock.update({}, ON_ITS_SIDE, STEP), settings.rolloverDamage);
	EXPECT_EQ(shock.update({}, ON_ITS_SIDE, STEP), 0.f) << "lying on its side is not a new rollover";
}

TEST(CargoShock, ResetsAfterTeleportsAreNotCrashes)
{
	trok::CargoShock shock;

	shock.update(FORWARD * CRUISE_SPEED, UPRIGHT, STEP);
	shock.reset({}, UPRIGHT);

	EXPECT_EQ(shock.update({}, UPRIGHT, STEP), 0.f);
}

TEST(Deliveries, CargoIsOnlyDamagedOnceLoaded)
{
	trok::Deliveries deliveries = WithTowns({ A, B, C });
	deliveries.update(OUTSIDE_A, FRAME);
	deliveries.damageCargo(HALF_DAMAGE);

	ASSERT_TRUE(deliveries.getDelivery().has_value());
	EXPECT_EQ(deliveries.getDelivery()->condition, 1.f) << "nothing is loaded before the pickup";
}

TEST(Deliveries, DamagedCargoPaysLess)
{
	trok::Deliveries intact  = Loaded();
	trok::Deliveries damaged = Loaded();

	damaged.damageCargo(HALF_DAMAGE);

	const std::optional<int64_t> full_payout = intact.update(B.center, FRAME);
	const std::optional<int64_t> half_payout = damaged.update(B.center, FRAME);

	ASSERT_TRUE(full_payout.has_value());
	ASSERT_TRUE(half_payout.has_value());
	EXPECT_EQ(*half_payout, std::llround(static_cast<double>(*full_payout) * HALF_DAMAGE));
	EXPECT_EQ(damaged.getDelivery()->condition, 1.f) << "the next job starts with undamaged cargo";
}

TEST(Deliveries, CargoConditionNeverDropsBelowZero)
{
	trok::Deliveries deliveries = Loaded();
	deliveries.damageCargo(HALF_DAMAGE * 3.f);

	EXPECT_EQ(deliveries.getDelivery()->condition, 0.f);
}
