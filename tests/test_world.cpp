#include <lunar/world/chunk_storage.hpp>
#include <lunar/world/region.hpp>
#include <lunar/world/region_store.hpp>
#include <lunar/world/terrain_generator.hpp>
#include <lunar/world/world_storage.hpp>
#include <lunar/file/text_file.hpp>
#include <gtest/gtest.h>

#include "temporary_directory.hpp"
#include "world_test_types.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace
{
	using namespace lunar::World;

	constexpr uint32_t GENERATOR_VERSION     = 3;
	constexpr int32_t  WORLD_SEED            = 777;
	constexpr int32_t  OTHER_WORLD_SEED      = 888;
	constexpr size_t   REGIONS_AROUND_ORIGIN = 9;
	constexpr size_t   SINGLE_REGION         = 1;
	constexpr size_t   TWO_REGIONS           = 2;
	constexpr size_t   FOUR_REGIONS          = 4;
	constexpr size_t   WORKERS               = 4;
	constexpr int32_t  INTERIOR_CHUNK        = 60;
	constexpr int32_t  WIDE_SAMPLE_REACH     = 5;
	constexpr int32_t  DIVISOR               = 128;
	constexpr int32_t  STORED_CHUNKS         = 64;
	constexpr int32_t  CHUNK_ROW             = 8;
	constexpr int      TORN_RECORD_BYTES     = 7;
	constexpr uint32_t OTHER_CHUNK_QUADS     = 16;
	constexpr float    HEIGHT_STEP           = 0.25f;

	const WorldSettings SETTINGS   = {};
	const WorldInfo     WORLD_INFO = { .seed = WORLD_SEED, .generatorVersion = GENERATOR_VERSION };
	const glm::vec3     FAR_AWAY   = { 1.0e6f, 0.f, 1.0e6f };
	const std::string   CORRUPT    = "{ this is not json";

	void FinishJobs(lunar::JobSystem& jobs)
	{
		jobs.waitIdle();
		jobs.processCompleted();
	}

	void WriteText(const Fs::Path& path, const std::string& text)
	{
		Fs::TextFile file;
		file.content = text;
		ASSERT_TRUE(file.toFile(path));
	}

	Heightmap NumberedHeightmap(float first_value)
	{
		const size_t samples_per_side = HeightmapSamplesPerSide(SETTINGS);
		Heightmap    heightmap        = { .samplesPerSide = HeightmapSamplesPerSide(SETTINGS) };

		for (size_t index = 0; index < samples_per_side * samples_per_side; index++)
			heightmap.heights.push_back(first_value + static_cast<float>(index) * HEIGHT_STEP);

		return heightmap;
	}

	std::shared_ptr<const WorldStorage> CreateStorage(const TemporaryDirectory& directory)
	{
		return std::make_shared<const WorldStorage>(*WorldStorage::create(directory.getPath(), WORLD_INFO));
	}
}

TEST(WorldGrid, FloorDivideRoundsTowardsNegativeInfinity)
{
	EXPECT_EQ(FloorDivide(0,            DIVISOR), 0);
	EXPECT_EQ(FloorDivide(DIVISOR - 1,  DIVISOR), 0);
	EXPECT_EQ(FloorDivide(DIVISOR,      DIVISOR), 1);
	EXPECT_EQ(FloorDivide(-1,           DIVISOR), -1);
	EXPECT_EQ(FloorDivide(-DIVISOR,     DIVISOR), -1);
	EXPECT_EQ(FloorDivide(-DIVISOR - 1, DIVISOR), -2);
}

TEST(WorldGrid, RegionAtMapsChunksAndPositions)
{
	const int32_t region_chunks = static_cast<int32_t>(SETTINGS.regionChunks);

	EXPECT_EQ(RegionAt(ChunkCoord { 0, region_chunks - 1 }, SETTINGS), (RegionCoord { 0, 0 }));
	EXPECT_EQ(RegionAt(ChunkCoord { region_chunks, -1 },     SETTINGS), (RegionCoord { 1, -1 }));
	EXPECT_EQ(RegionAt(glm::vec3(-1.f, 0.f, 1.f),            SETTINGS), (RegionCoord { -1, 0 }));
}

TEST(WorldGrid, ChunkWithinRegionIsAlwaysNonNegative)
{
	const int32_t region_chunks = static_cast<int32_t>(SETTINGS.regionChunks);

	EXPECT_EQ(ChunkWithinRegion({ 5, region_chunks + 2 }, SETTINGS), (ChunkCoord { 5, 2 }));
	EXPECT_EQ(ChunkWithinRegion({ -1, -region_chunks },    SETTINGS), (ChunkCoord { region_chunks - 1, 0 }));
}

TEST(WorldGrid, ChunksNeedNeighbourRegionsOnlyAtBorders)
{
	const int32_t last_chunk = static_cast<int32_t>(SETTINGS.regionChunks) - 1;

	EXPECT_EQ(RegionsNeededForChunk({ INTERIOR_CHUNK, INTERIOR_CHUNK }, SETTINGS).size(), SINGLE_REGION);
	EXPECT_EQ(RegionsNeededForChunk({ last_chunk,     INTERIOR_CHUNK }, SETTINGS).size(), TWO_REGIONS);
	EXPECT_EQ(RegionsNeededForChunk({ 0,              0              }, SETTINGS).size(), FOUR_REGIONS);
}

TEST(WorldGrid, ASampleReachPullsInFurtherRegions)
{
	const WorldSettings reaching     = { .sampleReachChunks = WIDE_SAMPLE_REACH };
	const int32_t       inside_reach = static_cast<int32_t>(reaching.regionChunks) - WIDE_SAMPLE_REACH;

	EXPECT_EQ(RegionsNeededForChunk({ INTERIOR_CHUNK, INTERIOR_CHUNK }, reaching).size(), SINGLE_REGION);
	EXPECT_EQ(RegionsNeededForChunk({ inside_reach,   INTERIOR_CHUNK }, reaching).size(), TWO_REGIONS);
	EXPECT_EQ(RegionsNeededForChunk({ inside_reach,   INTERIOR_CHUNK }, SETTINGS).size(), SINGLE_REGION);
}

TEST(RegionContext, FindsOnlyTheRegionsItWasGiven)
{
	const RegionContext<TestPlan> context(SETTINGS, {
		{ { -1, 0 }, std::make_shared<const TestPlan>(TestPlan { 1 }) },
		{ { 0,  0 }, std::make_shared<const TestPlan>(TestPlan { 2 }) }
	});

	ASSERT_NE(context.findRegion({ -1, 0 }), nullptr);
	EXPECT_EQ(context.findRegion({ -1, 0 })->value, 1);
	EXPECT_EQ(context.findRegion({ 0,  0 })->value, 2);
	EXPECT_EQ(context.findRegion({ 0,  1 }), nullptr);
}

TEST(WorldStorage, CreatedWorldCanBeReopened)
{
	const TemporaryDirectory directory;

	ASSERT_TRUE(WorldStorage::create(directory.getPath(), WORLD_INFO).has_value());

	const std::optional<WorldStorage> reopened = WorldStorage::open(directory.getPath());
	ASSERT_TRUE(reopened.has_value());
	EXPECT_EQ(reopened->getInfo(),      WORLD_INFO);
	EXPECT_TRUE(reopened->getDirectory() == directory.getPath());
}

TEST(WorldStorage, ExistingWorldIsNeitherRecreatedNorOverwritten)
{
	const TemporaryDirectory directory;
	ASSERT_TRUE(WorldStorage::create(directory.getPath(), WORLD_INFO).has_value());

	EXPECT_FALSE(WorldStorage::create(directory.getPath(), { .seed = OTHER_WORLD_SEED }).has_value());

	const std::optional<WorldStorage> continued = WorldStorage::openOrCreate(directory.getPath(), { .seed = OTHER_WORLD_SEED });
	ASSERT_TRUE(continued.has_value());
	EXPECT_EQ(continued->getInfo(), WORLD_INFO);
}

TEST(WorldStorage, MissingOrCorruptWorldCannotBeOpened)
{
	const TemporaryDirectory missing;
	const TemporaryDirectory corrupt;
	WriteText(corrupt.getPath() / "world.json", CORRUPT);

	EXPECT_FALSE(WorldStorage::open(missing.getPath()).has_value());
	EXPECT_FALSE(WorldStorage::open(corrupt.getPath()).has_value());
}

TEST(WorldStorage, RegionsRoundTripIncludingNegativeCoordinates)
{
	const TemporaryDirectory directory;
	const auto               storage = CreateStorage(directory);

	for (const RegionCoord coord : { RegionCoord { 0, 0 }, RegionCoord { -5, 12 } })
	{
		const TestPlan plan = { TestPlan::ValueFor(coord) };

		EXPECT_FALSE(storage->loadRegion<TestPlan>(coord).has_value());
		ASSERT_TRUE(storage->saveRegion(coord, plan));
		EXPECT_EQ(storage->loadRegion<TestPlan>(coord), plan);
	}
}

TEST(WorldStorage, CorruptMismatchedOrInvalidRegionsAreRejected)
{
	const TemporaryDirectory directory;
	const auto               storage = CreateStorage(directory);

	ASSERT_TRUE(storage->saveRegion(RegionCoord { 1, 1 }, TestPlan { 1 }));
	std::filesystem::rename(directory.getPath() / "regions" / "1.1.json", directory.getPath() / "regions" / "2.2.json");
	WriteText(directory.getPath() / "regions" / "3.3.json", CORRUPT);
	ASSERT_TRUE(storage->saveRegionJson({ 4, 4 }, { { "value", -1 } }));
	ASSERT_TRUE(storage->saveRegionJson({ 5, 5 }, { { "unexpected", 1 } }));

	EXPECT_FALSE(storage->loadRegion<TestPlan>({ 2, 2 }).has_value());
	EXPECT_FALSE(storage->loadRegion<TestPlan>({ 3, 3 }).has_value());
	EXPECT_FALSE(storage->loadRegion<TestPlan>({ 4, 4 }).has_value());
	EXPECT_FALSE(storage->loadRegion<TestPlan>({ 5, 5 }).has_value());
}

TEST(RegionStore, RegionsArePlannedOnceAndLoadedAfterwards)
{
	const TemporaryDirectory directory;
	lunar::JobSystem         jobs(WORKERS);
	const auto               storage = CreateStorage(directory);
	const auto               planner = std::make_shared<const TestPlanner>();
	{
		RegionStore<TestPlan> regions(jobs, storage, planner, SETTINGS);
		regions.update({});
		EXPECT_EQ(regions.getPendingRegionCount(), REGIONS_AROUND_ORIGIN);

		FinishJobs(jobs);
		EXPECT_EQ(regions.getLoadedRegionCount(), REGIONS_AROUND_ORIGIN);
	}

	EXPECT_EQ(planner->planCount,    static_cast<int>(REGIONS_AROUND_ORIGIN));
	EXPECT_EQ(planner->restoreCount, 0);

	RegionStore<TestPlan> regions(jobs, storage, planner, SETTINGS);
	regions.update({});
	FinishJobs(jobs);

	EXPECT_EQ(planner->planCount,    static_cast<int>(REGIONS_AROUND_ORIGIN));
	EXPECT_EQ(planner->restoreCount, static_cast<int>(REGIONS_AROUND_ORIGIN));
	ASSERT_NE(regions.find({ -1, 1 }), nullptr);
	EXPECT_EQ(regions.find({ -1, 1 })->value, TestPlan::ValueFor({ -1, 1 }));
}

TEST(RegionStore, ContextIsAvailableOnceAllNeededRegionsAreLoaded)
{
	const TemporaryDirectory directory;
	lunar::JobSystem         jobs(WORKERS);
	RegionStore<TestPlan>    regions(jobs, CreateStorage(directory), std::make_shared<const TestPlanner>(), SETTINGS);

	EXPECT_FALSE(regions.gatherContext({}).has_value());
	EXPECT_EQ(regions.getPendingRegionCount(), FOUR_REGIONS);

	FinishJobs(jobs);

	const std::optional<RegionContext<TestPlan>> context = regions.gatherContext({});
	ASSERT_TRUE(context.has_value());
	EXPECT_NE(context->findRegion({ -1, -1 }), nullptr);
	EXPECT_NE(context->findRegion({ 0,  0 }),  nullptr);
}

TEST(RegionStore, RegionsLeftBehindAreUnloaded)
{
	const TemporaryDirectory directory;
	lunar::JobSystem         jobs(WORKERS);
	RegionStore<TestPlan>    regions(jobs, CreateStorage(directory), std::make_shared<const TestPlanner>(), SETTINGS);

	regions.update({});
	FinishJobs(jobs);

	regions.update(FAR_AWAY);
	EXPECT_EQ(regions.find({}), nullptr);
	EXPECT_EQ(regions.getLoadedRegionCount(),  0u);
	EXPECT_EQ(regions.getPendingRegionCount(), REGIONS_AROUND_ORIGIN);

	FinishJobs(jobs);
	EXPECT_EQ(regions.getLoadedRegionCount(), REGIONS_AROUND_ORIGIN);
}

TEST(RegionStore, DestroyingTheStoreCancelsPendingRegions)
{
	const TemporaryDirectory directory;
	lunar::JobSystem         jobs(WORKERS);
	{
		RegionStore<TestPlan> regions(jobs, CreateStorage(directory), std::make_shared<const TestPlanner>(), SETTINGS);
		regions.update({});
	}

	jobs.waitIdle();
	EXPECT_EQ(jobs.processCompleted(), 0u);
}

TEST(ChunkStorage, HeightmapsRoundTripExactlyAcrossRegionsAndInstances)
{
	const TemporaryDirectory directory;
	const ChunkCoord         negative = { -1, -300 };
	const ChunkCoord         positive = { 2, 3 };
	{
		const ChunkStorage storage(directory.getPath(), SETTINGS);

		EXPECT_FALSE(storage.load(negative).has_value());
		ASSERT_TRUE(storage.save(negative, NumberedHeightmap(-50.f)));
		ASSERT_TRUE(storage.save(positive, NumberedHeightmap(10.f)));
		EXPECT_EQ(storage.load(negative), NumberedHeightmap(-50.f));
	}

	const ChunkStorage reopened(directory.getPath(), SETTINGS);
	EXPECT_EQ(reopened.load(negative), NumberedHeightmap(-50.f));
	EXPECT_EQ(reopened.load(positive), NumberedHeightmap(10.f));
	EXPECT_FALSE(reopened.load({ 2, 4 }).has_value());
}

TEST(ChunkStorage, FirstStoredHeightmapWins)
{
	const TemporaryDirectory directory;
	const ChunkStorage       storage(directory.getPath(), SETTINGS);

	ASSERT_TRUE(storage.save({}, NumberedHeightmap(1.f)));
	EXPECT_TRUE(storage.save({}, NumberedHeightmap(2.f)));

	EXPECT_EQ(storage.load({}), NumberedHeightmap(1.f));
}

TEST(ChunkStorage, TornRecordIsDiscardedAndOverwritten)
{
	const TemporaryDirectory directory;
	{
		const ChunkStorage storage(directory.getPath(), SETTINGS);
		ASSERT_TRUE(storage.save({ 0, 0 }, NumberedHeightmap(1.f)));
		ASSERT_TRUE(storage.save({ 0, 1 }, NumberedHeightmap(2.f)));
	}

	const Fs::Path region_file = directory.getPath() / "0.0.chunks";
	std::filesystem::resize_file(region_file, std::filesystem::file_size(region_file) - TORN_RECORD_BYTES);

	const ChunkStorage storage(directory.getPath(), SETTINGS);
	EXPECT_EQ(storage.load({ 0, 0 }), NumberedHeightmap(1.f));
	EXPECT_FALSE(storage.load({ 0, 1 }).has_value());

	ASSERT_TRUE(storage.save({ 0, 2 }, NumberedHeightmap(3.f)));

	const ChunkStorage reopened(directory.getPath(), SETTINGS);
	EXPECT_EQ(reopened.load({ 0, 0 }), NumberedHeightmap(1.f));
	EXPECT_EQ(reopened.load({ 0, 2 }), NumberedHeightmap(3.f));
}

TEST(ChunkStorage, FilesFromDifferentChunkSizesAreNeitherReadNorOverwritten)
{
	const TemporaryDirectory directory;
	{
		const ChunkStorage storage(directory.getPath(), SETTINGS);
		ASSERT_TRUE(storage.save({}, NumberedHeightmap(1.f)));
	}

	const WorldSettings other_settings = { .chunkQuads = OTHER_CHUNK_QUADS };
	const ChunkStorage  other_storage(directory.getPath(), other_settings);
	Heightmap           other_heightmap = { .samplesPerSide = HeightmapSamplesPerSide(other_settings) };
	other_heightmap.heights.resize(static_cast<size_t>(other_heightmap.samplesPerSide) * other_heightmap.samplesPerSide);

	EXPECT_FALSE(other_storage.load({}).has_value());
	EXPECT_FALSE(other_storage.save({ 1, 0 }, other_heightmap));

	const ChunkStorage storage(directory.getPath(), SETTINGS);
	EXPECT_EQ(storage.load({}), NumberedHeightmap(1.f));
}

TEST(ChunkStorage, ConcurrentSavesAreAllKept)
{
	const TemporaryDirectory directory;
	lunar::JobSystem         jobs(WORKERS);
	const auto               storage = std::make_shared<const ChunkStorage>(directory.getPath(), SETTINGS);

	for (int32_t index = 0; index < STORED_CHUNKS; index++)
		jobs.submit([storage, index] { return storage->save({ index % CHUNK_ROW, index / CHUNK_ROW }, NumberedHeightmap(static_cast<float>(index))); }, [](bool) {});

	FinishJobs(jobs);

	const ChunkStorage reopened(directory.getPath(), SETTINGS);
	for (int32_t index = 0; index < STORED_CHUNKS; index++)
		EXPECT_EQ(reopened.load({ index % CHUNK_ROW, index / CHUNK_ROW }), NumberedHeightmap(static_cast<float>(index)));
}

TEST(TerrainPersistence, GeneratedChunksAreStoredAndReusedWithoutGenerating)
{
	const TemporaryDirectory      directory;
	const ChunkStorage            storage(directory.getPath(), SETTINGS);
	const WaveGenerator           generator;
	const RegionContext<TestPlan> context(SETTINGS, {});
	const ChunkCoord              coord = { 3, -2 };

	const ChunkData generated     = LoadOrGenerateChunk(generator, storage, context, coord, SETTINGS);
	const int       samples_taken = generator.sampleCount;
	const ChunkData loaded        = LoadOrGenerateChunk(generator, storage, context, coord, SETTINGS);

	EXPECT_GT(samples_taken, 0);
	EXPECT_EQ(generator.sampleCount, samples_taken);
	EXPECT_EQ(loaded.heightmap, generated.heightmap);
	EXPECT_EQ(loaded.mesh.vertices.size(), generated.mesh.vertices.size());
	EXPECT_EQ(loaded.mesh.indices,         generated.mesh.indices);
}
