#include <lunar/core/scene.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	struct Position
	{
		float x = 0.f;
		float y = 0.f;
	};

	struct Velocity
	{
		float dx = 0.f;
		float dy = 0.f;
	};

	struct Unused {};

	struct Loaded : lunar::Component_T
	{
		Loaded(float loaded_value) : value(loaded_value) {}

		static Loaded Deserialize(const nlohmann::json& json)
		{
			return Loaded(json["value"]);
		}

		static nlohmann::json Serialize(const Loaded& component)
		{
			return { { "value", component.value } };
		}

		float value = 0.f;
	};

	constexpr const char* SCENE_FILE_NAME = "lunar_test_scene.json";
	constexpr const char* SCENE_FILE_CONTENT = R"({
		"name": "Test Scene",
		"gameObjects": [
			{
				"name": "Parent",
				"transform": { "position": { "x": 1.0, "y": 2.0, "z": 3.0 } },
				"components": [ { "type": "loaded", "value": 42.0 }, { "type": "unknown" } ],
				"children": [ { "name": "Child" } ]
			}
		]
	})";
}

TEST(Scene, AddedComponentCanBeRetrieved)
{
	lunar::Scene scene;

	const lunar::Entity entity = scene.createEntity();
	scene.addComponent<Position>(entity, 1.f, 2.f);

	EXPECT_TRUE(scene.hasComponent<Position>(entity));
	ASSERT_NE(scene.getComponent<Position>(entity), nullptr);
	EXPECT_FLOAT_EQ(scene.getComponent<Position>(entity)->y, 2.f);
}

TEST(Scene, MissingComponentIsNotFound)
{
	lunar::Scene scene;

	const lunar::Entity entity = scene.createEntity();
	scene.addComponent<Position>(entity);

	EXPECT_FALSE(scene.hasComponent<Velocity>(entity));
	EXPECT_EQ(scene.getComponent<Velocity>(entity), nullptr);
	EXPECT_EQ(scene.getComponent<Unused>(entity), nullptr);
}

TEST(Scene, RemovedComponentIsGone)
{
	lunar::Scene scene;

	const lunar::Entity entity = scene.createEntity();
	scene.addComponent<Position>(entity);
	scene.addComponent<Velocity>(entity);

	scene.removeComponent<Position>(entity);

	EXPECT_FALSE(scene.hasComponent<Position>(entity));
	EXPECT_TRUE(scene.hasComponent<Velocity>(entity));
}

TEST(Scene, ForEachVisitsOnlyEntitiesWithAllTypes)
{
	lunar::Scene scene;

	const lunar::Entity moving   = scene.createEntity();
	const lunar::Entity still    = scene.createEntity();
	const lunar::Entity velocity = scene.createEntity();
	scene.addComponent<Position>(moving);
	scene.addComponent<Velocity>(moving);
	scene.addComponent<Position>(still);
	scene.addComponent<Velocity>(velocity);

	int visits = 0;
	scene.forEach<Position, Velocity>([&](lunar::Entity entity, Position&, Velocity&) {
		EXPECT_EQ(entity, moving);
		visits++;
	});

	EXPECT_EQ(visits, 1);
}

TEST(Scene, ForEachCanModifyComponents)
{
	lunar::Scene scene;

	const lunar::Entity entity = scene.createEntity();
	scene.addComponent<Position>(entity, 1.f, 0.f);
	scene.addComponent<Velocity>(entity, 10.f, 0.f);

	scene.forEach<Position, Velocity>([](lunar::Entity, Position& position, Velocity& velocity) {
		position.x += velocity.dx;
	});

	EXPECT_FLOAT_EQ(scene.getComponent<Position>(entity)->x, 11.f);
}

TEST(Scene, ForEachWithNeverAddedTypeVisitsNothing)
{
	lunar::Scene scene;

	scene.addComponent<Position>(scene.createEntity());

	int visits = 0;
	scene.forEach<Position, Unused>([&](lunar::Entity, Position&, Unused&) { visits++; });

	EXPECT_EQ(visits, 0);
}

TEST(Scene, DestroyIsDeferredUntilFlush)
{
	lunar::Scene scene;

	const lunar::Entity entity = scene.createEntity();
	scene.addComponent<Position>(entity);

	scene.destroyEntity(entity);

	EXPECT_TRUE(entity.valid());
	EXPECT_TRUE(scene.hasComponent<Position>(entity));

	scene.flushDestroyedEntities();

	EXPECT_FALSE(entity.valid());
	EXPECT_FALSE(scene.hasComponent<Position>(entity));
}

TEST(Scene, DestroyingTwiceIsHarmless)
{
	lunar::Scene scene;

	const lunar::Entity destroyed = scene.createEntity();
	const lunar::Entity kept      = scene.createEntity();
	scene.addComponent<Position>(kept, 7.f, 7.f);

	scene.destroyEntity(destroyed);
	scene.destroyEntity(destroyed);
	scene.flushDestroyedEntities();

	EXPECT_TRUE(kept.valid());
	EXPECT_FLOAT_EQ(scene.getComponent<Position>(kept)->x, 7.f);
}

TEST(Scene, EntityReusingSlotStartsWithoutComponents)
{
	lunar::Scene scene;

	const lunar::Entity old_entity = scene.createEntity();
	scene.addComponent<Position>(old_entity);
	scene.addComponent<Velocity>(old_entity);
	scene.destroyEntity(old_entity);
	scene.flushDestroyedEntities();

	const lunar::Entity new_entity = scene.createEntity();

	EXPECT_EQ(new_entity.getIndex(), old_entity.getIndex());
	EXPECT_FALSE(scene.hasComponent<Position>(new_entity));
	EXPECT_FALSE(scene.hasComponent<Velocity>(new_entity));
}

TEST(Scene, EntityFromAnotherSceneIsRejected)
{
	lunar::Scene scene;
	lunar::Scene other_scene;

	scene.addComponent<Position>(scene.createEntity());
	const lunar::Entity foreign = other_scene.createEntity();

	EXPECT_FALSE(scene.hasComponent<Position>(foreign));
	EXPECT_EQ(scene.getComponent<Position>(foreign), nullptr);
}

TEST(SceneLoader, LoadsObjectsTransformsAndComponents)
{
	const auto scene_path = std::filesystem::temp_directory_path() / SCENE_FILE_NAME;
	std::ofstream(scene_path) << SCENE_FILE_CONTENT;

	lunar::Scene scene;
	lunar::SceneLoader()
		.destination(scene)
		.useClassSerializer<Loaded>("loaded")
		.loadJsonFile(scene_path);

	std::filesystem::remove(scene_path);

	EXPECT_EQ(scene.getName(), "Test Scene");

	lunar::GameObject parent = scene.getGameObject("Parent");
	lunar::GameObject child  = scene.getGameObject("Child");
	ASSERT_FALSE(parent == nullptr);
	ASSERT_FALSE(child == nullptr);

	EXPECT_EQ(child->getParent(), parent);
	EXPECT_FLOAT_EQ(parent->getTransform().position.z, 3.f);

	ASSERT_NE(parent->getComponent<Loaded>(), nullptr);
	EXPECT_FLOAT_EQ(parent->getComponent<Loaded>()->value, 42.f);
	EXPECT_EQ(parent->getComponent<Loaded>()->getGameObject(), parent);
}
