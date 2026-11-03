#include <lunar/render/model.hpp>
#include <lunar/world/decoration.hpp>
#include <lunar/world/decoration_world.hpp>
#include <lunar/world/grid.hpp>
#include <lunar/world/terrain.hpp>
#include <lunar/file/json_file.hpp>
#include <gtest/gtest.h>

#include "temporary_directory.hpp"
#include "test_models.hpp"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <optional>
#include <vector>

namespace
{
	using namespace lunar::World;

	constexpr float        TOLERANCE      = 1e-5f;
	constexpr DecorationId LARGE_ID       = 0xFEDCBA9876543210ull;
	constexpr DecorationId SMALL_ID       = 7;
	constexpr float        TURN           = 0.5f;
	constexpr float        PLACED_SCALE   = 1.25f;
	constexpr int32_t      SQUARE_RADIUS  = 2;
	constexpr size_t       SQUARE_COUNT   = 25;
	constexpr float        SLOPE          = 0.5f;
	constexpr float        LIFT           = 1.f;
	constexpr float        SAMPLE_X       = 3.f;
	constexpr float        SAMPLE_Z       = 5.f;

	const glm::vec3  UP              = { 0.f, 1.f, 0.f };
	const glm::dvec3 FAR_POSITION    = { 1234567.25, 3.5, -7654321.75 };
	const ChunkCoord SQUARE_CENTRE   = { 3, -2 };
	const glm::vec4  BASE_COLOR      = { 0.5f, 1.f, 1.f, 1.f };
	const glm::vec4  VERTEX_COLOR    = { 1.f, 0.5f, 0.25f, 1.f };
	const glm::vec3  MODEL_OFFSET    = { 0.f, 1.f, 0.f };

	DecorationCatalog TwoKinds()
	{
		return DecorationCatalog
		{
			.types =
			{
				{ .name = "test:rock",    .lods = { { "rock.gltf", 100.f } } },
				{ .name = "test:sapling", .lods = { { "sapling.gltf", 50.f }, { "sapling_far.gltf", 200.f } }, .breakable = true, .mass = 10.f }
			}
		};
	}

	std::optional<DecorationCatalog> RoundTrip(const DecorationCatalog& catalog)
	{
		return Fs::DeserializeJson<DecorationCatalog>(DecorationCatalog::Serialize(catalog));
	}

	std::optional<lunar::Render::ModelData> WriteAndLoad(const TemporaryDirectory& directory, const TestModel& model)
	{
		const Fs::Path path = directory.getPath() / "model.gltf";
		model.write(path);
		return lunar::Render::LoadModel(path);
	}
}

TEST(DecorationCatalog, RoundTripsThroughJson)
{
	EXPECT_EQ(RoundTrip(TwoKinds()), TwoKinds());
}

TEST(DecorationCatalog, FindsKindsByName)
{
	const DecorationCatalog catalog = TwoKinds();

	EXPECT_EQ(catalog.find("test:sapling"), std::optional<DecorationKind>(1));
	EXPECT_FALSE(catalog.find("test:unknown").has_value());
}

TEST(DecorationCatalog, MalformedCatalogsAreRejected)
{
	DecorationCatalog duplicated = TwoKinds();
	duplicated.types[1].name = duplicated.types[0].name;

	DecorationCatalog unordered = TwoKinds();
	std::swap(unordered.types[1].lods[0], unordered.types[1].lods[1]);

	DecorationCatalog weightless = TwoKinds();
	weightless.types[1].mass = 0.f;

	DecorationCatalog overdetailed = TwoKinds();
	overdetailed.types[0].lods.clear();
	for (size_t level = 0; level <= lunar::MAX_INSTANCE_LODS; level++)
		overdetailed.types[0].lods.push_back({ "rock.gltf", static_cast<float>(level + 1) });

	DecorationCatalog invisible = TwoKinds();
	invisible.types[0].lods.clear();

	for (const DecorationCatalog& catalog : { duplicated, unordered, weightless, overdetailed, invisible })
		EXPECT_FALSE(RoundTrip(catalog).has_value());
}

TEST(DecorationChanges, RoundTripThroughJsonWithoutLosingPrecision)
{
	const DecorationChanges changes =
	{
		.removed = { LARGE_ID, SMALL_ID },
		.placed  = { { LARGE_ID, "test:sapling", FAR_POSITION, glm::angleAxis(TURN, UP), PLACED_SCALE } }
	};

	EXPECT_EQ(Fs::DeserializeJson<DecorationChanges>(DecorationChanges::Serialize(changes)), changes);
}

TEST(ModelLoading, RenderNodesKeepTheirTransformAndBakeTheirColors)
{
	const TemporaryDirectory                      directory;
	const std::optional<lunar::Render::ModelData> model = WriteAndLoad(directory, { .translation = MODEL_OFFSET, .baseColor = BASE_COLOR, .vertexColor = VERTEX_COLOR, .collider = false });
	ASSERT_TRUE(model.has_value());

	EXPECT_EQ(model->mesh.vertices.size(), TestModel::CORNERS);
	EXPECT_EQ(model->mesh.indices.size(),  TestModel::INDEX_COUNT);
	EXPECT_TRUE(model->colliders.empty());

	for (const lunar::Render::Vertex& vertex : model->mesh.vertices)
	{
		EXPECT_NEAR(glm::distance(vertex.color, BASE_COLOR * VERTEX_COLOR), 0.f, TOLERANCE);
		EXPECT_NEAR(std::abs(vertex.position.y - MODEL_OFFSET.y), TestModel {}.halfExtent, TOLERANCE);
		EXPECT_GT(glm::dot(vertex.normal, vertex.position - MODEL_OFFSET), 0.f);
	}
}

TEST(ModelLoading, ColliderNodesBecomeHullsInsteadOfGeometry)
{
	const TemporaryDirectory                      directory;
	const std::optional<lunar::Render::ModelData> model = WriteAndLoad(directory, { .translation = MODEL_OFFSET });
	ASSERT_TRUE(model.has_value());
	ASSERT_EQ(model->colliders.size(), 1u);

	EXPECT_EQ(model->mesh.vertices.size(), TestModel::CORNERS) << "collider geometry must not be rendered";
	EXPECT_EQ(model->colliders.front().size(), TestModel::CORNERS);

	for (const glm::vec3& point : model->colliders.front())
		EXPECT_NEAR(std::abs(point.y - MODEL_OFFSET.y), TestModel {}.halfExtent, TOLERANCE);
}

TEST(ModelLoading, MissingNormalsAreDerivedFromTheFaces)
{
	const TemporaryDirectory                      directory;
	const std::optional<lunar::Render::ModelData> model = WriteAndLoad(directory, { .normals = false, .collider = false });
	ASSERT_TRUE(model.has_value());

	for (const lunar::Render::Vertex& vertex : model->mesh.vertices)
	{
		EXPECT_NEAR(glm::length(vertex.normal), 1.f, TOLERANCE);
		EXPECT_GT(glm::dot(vertex.normal, vertex.position), 0.f) << "normals point away from the model";
	}
}

TEST(ModelLoading, MissingFilesAreReportedAsFailures)
{
	const TemporaryDirectory directory;

	EXPECT_FALSE(lunar::Render::LoadModel(directory.getPath() / "missing.glb").has_value());
}

TEST(Heightmap, HeightsAreInterpolatedBetweenSamples)
{
	const WorldSettings settings;
	const Heightmap     heightmap = SampleHeightmap([](double x, double) { return static_cast<float>(x) * SLOPE + LIFT; }, ChunkCoord {}, settings);

	EXPECT_NEAR(HeightmapHeightAt(heightmap, { SAMPLE_X, SAMPLE_Z }, settings), SAMPLE_X * SLOPE + LIFT, TOLERANCE);
}

TEST(WorldGrid, SquareAroundCoversTheWholeSquare)
{
	const std::vector<ChunkCoord> coords = SquareAround(SQUARE_CENTRE, SQUARE_RADIUS);

	EXPECT_EQ(coords.size(), SQUARE_COUNT);
	EXPECT_TRUE(std::ranges::all_of(coords, [](ChunkCoord coord) { return IsInsideSquare(coord, SQUARE_CENTRE, SQUARE_RADIUS); }));
}
