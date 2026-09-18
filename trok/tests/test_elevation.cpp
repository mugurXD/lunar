#include <trok/world/elevation.hpp>
#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

namespace
{
	const trok::ElevationCurve CURVE = { .points = { { -0.5f, 0.f }, { 0.f, 10.f }, { 0.5f, 110.f } } };
}

TEST(Elevation, HeightsInterpolateBetweenPoints)
{
	EXPECT_FLOAT_EQ(CURVE.heightAt(0.f),   10.f);
	EXPECT_FLOAT_EQ(CURVE.heightAt(-0.25f), 5.f);
	EXPECT_FLOAT_EQ(CURVE.heightAt(0.25f), 60.f);
}

TEST(Elevation, HeightsHoldTheirEndValuesOutsideTheCurve)
{
	EXPECT_FLOAT_EQ(CURVE.heightAt(-1.f), 0.f);
	EXPECT_FLOAT_EQ(CURVE.heightAt(1.f),  110.f);
}

TEST(Elevation, CurvesRoundTripThroughJson)
{
	EXPECT_EQ(Fs::DeserializeJson<trok::ElevationCurve>(trok::ElevationCurve::Serialize(CURVE)), CURVE);
}

TEST(Elevation, MalformedCurvesAreRejected)
{
	nlohmann::json single_point = trok::ElevationCurve::Serialize(CURVE);
	single_point["points"].erase(1);
	single_point["points"].erase(1);

	nlohmann::json not_increasing = trok::ElevationCurve::Serialize(CURVE);
	not_increasing["points"][1][0] = not_increasing["points"][0][0];

	nlohmann::json missing_points = trok::ElevationCurve::Serialize(CURVE);
	missing_points.erase("points");

	nlohmann::json future_format = trok::ElevationCurve::Serialize(CURVE);
	future_format["formatVersion"] = future_format["formatVersion"].get<uint32_t>() + 1;

	EXPECT_FALSE(Fs::DeserializeJson<trok::ElevationCurve>(single_point).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::ElevationCurve>(not_increasing).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::ElevationCurve>(missing_points).has_value());
	EXPECT_FALSE(Fs::DeserializeJson<trok::ElevationCurve>(future_format).has_value());
}
