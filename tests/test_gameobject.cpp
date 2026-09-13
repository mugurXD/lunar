#include <lunar/core/scene.hpp>
#include <lunar/render/components.hpp>
#include <gtest/gtest.h>

namespace
{
	struct Position
	{
		float x = 0.f;
		float y = 0.f;
	};

	struct CountsStarts : lunar::Component_T
	{
		CountsStarts(int& start_count) : startCount(&start_count) {}

		void start() override
		{
			(*startCount)++;
		}

		int* startCount = nullptr;
	};

	constexpr int EXTRA_CAMERA_COUNT = 50;
}

TEST(GameObject, DefaultGameObjectIsNull)
{
	const lunar::GameObject object;

	EXPECT_TRUE(object == nullptr);
	EXPECT_FALSE(object.valid());
}

TEST(GameObject, CreatedObjectHasCoreComponents)
{
	lunar::Scene scene;

	const lunar::GameObject object = scene.createGameObject("Object");
	const lunar::Entity     entity = object.getEntity();

	EXPECT_TRUE(object.valid());
	EXPECT_EQ(object->getScene(), &scene);
	EXPECT_TRUE(scene.hasComponent<lunar::Name>(entity));
	EXPECT_TRUE(scene.hasComponent<lunar::Transform>(entity));
	EXPECT_TRUE(scene.hasComponent<lunar::Hierarchy>(entity));
}

TEST(GameObject, CanBeFoundByName)
{
	lunar::Scene scene;

	scene.createGameObject("First");
	const lunar::GameObject second = scene.createGameObject("Second");

	EXPECT_EQ(scene.getGameObject("Second"), second);
	EXPECT_EQ(scene.getGameObject("Second")->getName(), "Second");
	EXPECT_TRUE(scene.getGameObject("Missing") == nullptr);
}

TEST(GameObject, RootHasNoParent)
{
	lunar::Scene scene;

	const lunar::GameObject root = scene.createGameObject("Root");

	EXPECT_TRUE(root->getParent() == nullptr);
	EXPECT_TRUE(root->getChildren().empty());
}

TEST(GameObject, ChildrenAreKeptInCreationOrder)
{
	lunar::Scene scene;

	lunar::GameObject root   = scene.createGameObject("Root");
	lunar::GameObject first  = scene.createGameObject("First", root);
	lunar::GameObject second = root->createChildObject("Second");
	lunar::GameObject third  = scene.createGameObject("Third", root);

	const auto children = root->getChildren();

	ASSERT_EQ(children.size(), 3u);
	EXPECT_EQ(children[0], first);
	EXPECT_EQ(children[1], second);
	EXPECT_EQ(children[2], third);
	EXPECT_EQ(second->getParent(), root);
}

TEST(GameObject, WorldPositionFollowsParents)
{
	lunar::Scene scene;

	lunar::GameObject root       = scene.createGameObject("Root");
	lunar::GameObject child      = scene.createGameObject("Child", root);
	lunar::GameObject grandchild = scene.createGameObject("Grandchild", child);
	root->getTransform().position  = { 10.f, 0.f, 0.f };
	child->getTransform().position = { 1.f, 2.f, 3.f };

	const glm::vec3 world_position = grandchild->getWorldPos();

	EXPECT_FLOAT_EQ(world_position.x, 11.f);
	EXPECT_FLOAT_EQ(world_position.y, 2.f);
	EXPECT_FLOAT_EQ(world_position.z, 3.f);
}

TEST(GameObject, SetWorldPosConvertsToLocalSpace)
{
	lunar::Scene scene;

	lunar::GameObject root  = scene.createGameObject("Root");
	lunar::GameObject child = scene.createGameObject("Child", root);
	root->getTransform().position = { 10.f, 0.f, 0.f };

	child->setWorldPos({ 0.f, 0.f, 0.f });

	EXPECT_FLOAT_EQ(child->getLocalPos().x, -10.f);
	EXPECT_FLOAT_EQ(child->getWorldPos().x, 0.f);
}

TEST(GameObject, AddedComponentCanBeRetrieved)
{
	lunar::Scene scene;

	lunar::GameObject object   = scene.createGameObject("Object");
	Position*         position = object->addComponent<Position>(5.f, 6.f);

	ASSERT_NE(position, nullptr);
	EXPECT_EQ(object->getComponent<Position>(), position);
	EXPECT_FLOAT_EQ(object->getComponent<Position>()->y, 6.f);
}

TEST(GameObject, MissingComponentIsNull)
{
	lunar::Scene scene;

	lunar::GameObject with_component    = scene.createGameObject("With");
	lunar::GameObject without_component = scene.createGameObject("Without");
	with_component->addComponent<Position>();

	EXPECT_EQ(without_component->getComponent<Position>(), nullptr);
}

TEST(GameObject, ComponentBaseIsLinkedToItsObject)
{
	lunar::Scene scene;

	lunar::GameObject object = scene.createGameObject("Object");
	object->getTransform().position = { 4.f, 0.f, 0.f };

	lunar::Camera* camera = object->addComponent<lunar::Camera>();

	ASSERT_NE(camera, nullptr);
	EXPECT_EQ(camera->getGameObject(), object);
	EXPECT_EQ(camera->getScene(), &scene);
	EXPECT_FLOAT_EQ(camera->getTransform().position.x, 4.f);
}

TEST(GameObject, AddingComponentCallsStartOnce)
{
	lunar::Scene scene;
	int          start_count = 0;

	scene.createGameObject("Object")->addComponent<CountsStarts>(start_count);

	EXPECT_EQ(start_count, 1);
}

TEST(GameObject, MainCameraSurvivesAddingMoreCameras)
{
	lunar::Scene scene;

	lunar::GameObject main_object = scene.createGameObject("Main");
	scene.setMainCamera(main_object->addComponent<lunar::Camera>());

	for (int i = 0; i < EXTRA_CAMERA_COUNT; i++)
		scene.createGameObject("Extra")->addComponent<lunar::Camera>();

	ASSERT_NE(scene.getMainCamera(), nullptr);
	EXPECT_EQ(scene.getMainCamera()->getGameObject(), main_object);
}
