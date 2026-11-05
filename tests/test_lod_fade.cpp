#include <lunar/render/lod_fade.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using lunar::InstanceLod;
using lunar::MAX_INSTANCE_LODS;
using lunar::Render::LodDraw;
using lunar::Render::MeshHandle;
using lunar::Render::SelectLodDraws;

namespace
{
	constexpr MeshHandle NEAR_MESH    = { 1, 1 };
	constexpr MeshHandle FAR_MESH     = { 3, 1 };
	constexpr float      NEAR_END     = 100.f;
	constexpr float      NEAR_FADE    = 20.f;
	constexpr float      FAR_END      = 400.f;
	constexpr float      FAR_FADE     = 50.f;
	constexpr size_t     NEAR_LEVEL   = 0;
	constexpr size_t     MIDDLE_LEVEL = 1;
	constexpr size_t     FAR_LEVEL    = 2;

	using Lods  = std::array<InstanceLod, MAX_INSTANCE_LODS>;
	using Draws = std::array<LodDraw, MAX_INSTANCE_LODS>;

	const glm::vec2 NEAR_BAND = { NEAR_END - NEAR_FADE, NEAR_END };
	const glm::vec2 FAR_BAND  = { FAR_END - FAR_FADE, FAR_END };

	Lods NearAndFar(float near_fade = NEAR_FADE)
	{
		Lods lods = {};
		lods[NEAR_LEVEL] = { NEAR_MESH, NEAR_END, near_fade };
		lods[FAR_LEVEL]  = { FAR_MESH, FAR_END, FAR_FADE };
		return lods;
	}

	size_t DrawCount(const Draws& draws)
	{
		return static_cast<size_t>(std::ranges::count_if(draws, [](const LodDraw& draw) { return draw.mesh != MeshHandle {}; }));
	}
}

TEST(LodFade, CloseBatchesDrawOnlyTheFirstLevelWithoutFading)
{
	const Draws draws = SelectLodDraws(NearAndFar(), 10.f, 60.f);

	EXPECT_EQ(DrawCount(draws), 1u);
	EXPECT_EQ(draws[NEAR_LEVEL].mesh, NEAR_MESH);
	EXPECT_FALSE(draws[NEAR_LEVEL].fading);
}

TEST(LodFade, BatchesInABandDrawBothLevelsWithTheSameBand)
{
	const Draws draws = SelectLodDraws(NearAndFar(), 70.f, 90.f);

	EXPECT_EQ(DrawCount(draws), 2u);
	EXPECT_TRUE(draws[NEAR_LEVEL].fading);
	EXPECT_TRUE(draws[FAR_LEVEL].fading);
	EXPECT_EQ(glm::vec2(draws[NEAR_LEVEL].fadeBands.z, draws[NEAR_LEVEL].fadeBands.w), NEAR_BAND);
	EXPECT_EQ(glm::vec2(draws[FAR_LEVEL].fadeBands.x, draws[FAR_LEVEL].fadeBands.y), NEAR_BAND) << "the far level fades in exactly where the near one fades out";
}

TEST(LodFade, HardSwitchesStillSplitBatchesPerInstance)
{
	const Draws draws = SelectLodDraws(NearAndFar(0.f), 90.f, 110.f);

	EXPECT_EQ(DrawCount(draws), 2u);
	EXPECT_TRUE(draws[NEAR_LEVEL].fading);
	EXPECT_TRUE(draws[FAR_LEVEL].fading);
}

TEST(LodFade, TheLastLevelFadesOutAndThenStopsDrawing)
{
	const Draws fading = SelectLodDraws(NearAndFar(), 360.f, 380.f);

	EXPECT_EQ(DrawCount(fading), 1u);
	EXPECT_TRUE(fading[FAR_LEVEL].fading);
	EXPECT_EQ(glm::vec2(fading[FAR_LEVEL].fadeBands.z, fading[FAR_LEVEL].fadeBands.w), FAR_BAND);
	EXPECT_EQ(DrawCount(SelectLodDraws(NearAndFar(), FAR_END + 1.f, FAR_END + 50.f)), 0u);
}

TEST(LodFade, MissingLevelsHandOverToTheNextOne)
{
	Lods lods = NearAndFar();
	lods[MIDDLE_LEVEL] = { MeshHandle {}, (NEAR_END + FAR_END) * 0.5f, 0.f };

	const Draws draws = SelectLodDraws(lods, 85.f, 95.f);

	EXPECT_EQ(draws[MIDDLE_LEVEL].mesh, MeshHandle {});
	EXPECT_EQ(draws[FAR_LEVEL].mesh, FAR_MESH);
	EXPECT_EQ(glm::vec2(draws[FAR_LEVEL].fadeBands.x, draws[FAR_LEVEL].fadeBands.y), NEAR_BAND);
}
