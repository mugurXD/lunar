#include <lunar/render/instance_draw_table.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>

using lunar::Render::BuildInstanceDrawTable;
using lunar::Render::GpuInstanceDraw;
using lunar::Render::InstanceDraw;
using lunar::Render::InstanceDrawTable;
using lunar::Render::Mesh;

namespace
{
	constexpr uint32_t SMALL_BATCH  = 3;
	constexpr uint32_t MEDIUM_BATCH = 5;
	constexpr uint32_t LARGE_BATCH  = 7;
	constexpr uint64_t FIRST_SET    = 0x1000;
	constexpr uint64_t SECOND_SET   = 0x2000;
	constexpr uint64_t THIRD_SET    = 0x3000;
	constexpr size_t   GPU_DRAW_SIZE = 128;

	const glm::vec4 NEAR_BANDS = { -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 80.f, 100.f };
	const glm::vec4 FAR_BANDS  = { 80.f, 100.f, 350.f, 400.f };

	const Mesh NEAR_MESH = { .bounds = { glm::vec3(-1.f, 0.f, -2.f), glm::vec3(1.f, 4.f, 2.f) } };
	const Mesh FAR_MESH  = { .bounds = { glm::vec3(-1.f), glm::vec3(1.f) } };

	InstanceDraw Draw(const Mesh& mesh, uint64_t address, uint32_t count, const glm::vec4& bands, bool fading)
	{
		return { .mesh = &mesh, .instanceAddress = address, .instanceCount = count, .fadeBands = bands, .fading = fading };
	}
}

TEST(InstanceDrawTable, MatchesTheShaderLayout)
{
	EXPECT_EQ(sizeof(GpuInstanceDraw), GPU_DRAW_SIZE);
}

TEST(InstanceDrawTable, NoDrawsGiveAnEmptyTable)
{
	const InstanceDrawTable table = BuildInstanceDrawTable({});

	EXPECT_TRUE(table.draws.empty());
	EXPECT_TRUE(table.slots.empty());
	EXPECT_EQ(table.instanceCount, 0u);
}

TEST(InstanceDrawTable, DrawsOfTheSameSlotBecomeContiguous)
{
	const std::array draws =
	{
		Draw(NEAR_MESH, FIRST_SET, SMALL_BATCH, NEAR_BANDS, false),
		Draw(FAR_MESH, SECOND_SET, MEDIUM_BATCH, FAR_BANDS, false),
		Draw(NEAR_MESH, THIRD_SET, LARGE_BATCH, NEAR_BANDS, false)
	};

	const InstanceDrawTable table = BuildInstanceDrawTable(draws);

	ASSERT_EQ(table.slots.size(), 2u);
	ASSERT_EQ(table.draws.size(), draws.size());
	EXPECT_EQ(table.instanceCount, SMALL_BATCH + MEDIUM_BATCH + LARGE_BATCH);

	uint32_t expected_offset = 0;
	for (size_t index = 0; index < table.draws.size(); index++)
	{
		const GpuInstanceDraw& draw = table.draws[index];
		EXPECT_EQ(draw.outputOffset, expected_offset);
		EXPECT_TRUE(index == 0 || draw.slot >= table.draws[index - 1].slot);
		expected_offset += draw.instanceCount;
	}

	for (size_t slot = 0; slot < table.slots.size(); slot++)
	{
		const auto first = std::ranges::find(table.draws, slot, &GpuInstanceDraw::slot);
		ASSERT_NE(first, table.draws.end());
		EXPECT_EQ(table.slots[slot].firstInstance, first->outputOffset);
	}
}

TEST(InstanceDrawTable, SteadySlotsComeBeforeFadingOnes)
{
	const std::array draws =
	{
		Draw(NEAR_MESH, FIRST_SET, SMALL_BATCH, NEAR_BANDS, true),
		Draw(FAR_MESH, FIRST_SET, SMALL_BATCH, FAR_BANDS, true),
		Draw(NEAR_MESH, SECOND_SET, MEDIUM_BATCH, NEAR_BANDS, false)
	};

	const InstanceDrawTable table = BuildInstanceDrawTable(draws);

	ASSERT_EQ(table.slots.size(), 3u);
	EXPECT_FALSE(table.slots[0].fading);
	EXPECT_EQ(table.slots[0].mesh, &NEAR_MESH);
	EXPECT_EQ(table.slots[0].firstInstance, 0u);
	EXPECT_TRUE(table.slots[1].fading);
	EXPECT_TRUE(table.slots[2].fading);
}

TEST(InstanceDrawTable, TheSameMeshWithDifferentBandsGetsSeparateSlots)
{
	const std::array draws =
	{
		Draw(NEAR_MESH, FIRST_SET, SMALL_BATCH, NEAR_BANDS, false),
		Draw(NEAR_MESH, SECOND_SET, MEDIUM_BATCH, FAR_BANDS, false)
	};

	const InstanceDrawTable table = BuildInstanceDrawTable(draws);

	ASSERT_EQ(table.slots.size(), 2u);
	EXPECT_NE(table.slots[0].fadeBands, table.slots[1].fadeBands);
	EXPECT_EQ(table.draws[0].fadeBands, table.slots[table.draws[0].slot].fadeBands);
	EXPECT_EQ(table.draws[1].fadeBands, table.slots[table.draws[1].slot].fadeBands);
}

TEST(InstanceDrawTable, BoundingSpheresEncloseTheMeshBounds)
{
	const std::array draws = { Draw(NEAR_MESH, FIRST_SET, SMALL_BATCH, NEAR_BANDS, false) };

	const InstanceDrawTable table  = BuildInstanceDrawTable(draws);
	const glm::vec4&        sphere = table.draws.front().boundingSphere;

	EXPECT_EQ(glm::vec3(sphere), glm::vec3(0.f, 2.f, 0.f));
	EXPECT_FLOAT_EQ(sphere.w, 3.f);
}
