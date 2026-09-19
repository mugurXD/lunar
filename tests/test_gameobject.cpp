#include <lunar/core/scene.hpp>
#include <lunar/render/components.hpp>
#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

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

	struct Linked : lunar::Component_T {};

	struct CountsUpdates : lunar::Component_T
	{
		void update(const lunar::FrameTime& frame_time) override
		{
			updates++;
			lastDelta = frame_time.deltaTime;
		}

		int   updates   = 0;
		float lastDelta = 0.f;
	};

	struct AlsoCountsUpdates : lunar::Component_T
	{
		void update(const lunar::FrameTime&) override
		{
			updates++;
		}

		int updates = 0;
	};

	constexpr float TEST_DELTA_TIME = 0.25f;

	constexpr int   EXTRA_CAMERA_COUNT = 50;
	constexpr float FLOAT_TOLERANCE    = 1e-5f;
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
	EXPECT_TRUE(scene.hasComponent<lunar::WorldTransform>(entity));
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

	Linked* linked = object->addComponent<Linked>();

	ASSERT_NE(linked, nullptr);
	EXPECT_EQ(linked->getGameObject(), object);
	EXPECT_EQ(linked->getScene(), &scene);
	EXPECT_FLOAT_EQ(linked->getTransform().position.x, 4.f);
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
	main_object->addComponent<lunar::Camera>()->fov = 90.f;
	scene.setMainCamera(main_object);

	for (int i = 0; i < EXTRA_CAMERA_COUNT; i++)
		scene.createGameObject("Extra")->addComponent<lunar::Camera>();

	ASSERT_NE(scene.getMainCamera(), nullptr);
	EXPECT_EQ(scene.getMainCamera(), main_object->getComponent<lunar::Camera>());
	EXPECT_FLOAT_EQ(scene.getMainCamera()->fov, 90.f);
}

TEST(GameObject, MainCameraIsNullWithoutCamera)
{
	lunar::Scene scene;

	EXPECT_EQ(scene.getMainCamera(), nullptr);

	scene.setMainCamera(scene.createGameObject("No camera"));

	EXPECT_EQ(scene.getMainCamera(), nullptr);
}

TEST(Behaviour, UpdateRunsOncePerFrameForEveryInstance)
{
	lunar::Scene scene;

	lunar::GameObject first  = scene.createGameObject("First");
	lunar::GameObject second = scene.createGameObject("Second");
	first->addComponent<CountsUpdates>();
	second->addComponent<CountsUpdates>();

	scene.updateBehaviours(lunar::FrameTime{ .deltaTime = TEST_DELTA_TIME });
	scene.updateBehaviours(lunar::FrameTime{ .deltaTime = TEST_DELTA_TIME });

	EXPECT_EQ(first->getComponent<CountsUpdates>()->updates, 2);
	EXPECT_EQ(second->getComponent<CountsUpdates>()->updates, 2);
	EXPECT_FLOAT_EQ(first->getComponent<CountsUpdates>()->lastDelta, TEST_DELTA_TIME);
}

TEST(Behaviour, EveryBehaviourTypeIsUpdated)
{
	lunar::Scene scene;

	lunar::GameObject object = scene.createGameObject("Object");
	object->addComponent<CountsUpdates>();
	object->addComponent<AlsoCountsUpdates>();

	scene.updateBehaviours(lunar::FrameTime{});

	EXPECT_EQ(object->getComponent<CountsUpdates>()->updates, 1);
	EXPECT_EQ(object->getComponent<AlsoCountsUpdates>()->updates, 1);
}

TEST(Behaviour, DestroyedObjectsAreNotUpdated)
{
	lunar::Scene scene;

	lunar::GameObject kept      = scene.createGameObject("Kept");
	lunar::GameObject destroyed = scene.createGameObject("Destroyed");
	kept->addComponent<CountsUpdates>();
	destroyed->addComponent<CountsUpdates>();

	destroyed->destroy();
	scene.flushDestroyedEntities();
	scene.updateBehaviours(lunar::FrameTime{});

	EXPECT_EQ(kept->getComponent<CountsUpdates>()->updates, 1);
}

TEST(Camera, UpdateCamerasFollowsTransform)
{
	lunar::Scene scene;

	lunar::GameObject parent = scene.createGameObject("Parent");
	lunar::GameObject object = scene.createGameObject("Camera", parent);
	parent->getTransform().position = { 0.f, 5.f, 0.f };
	object->getTransform().position = { 1.f, 0.f, 0.f };
	object->getTransform().rotation = lunar::CameraRotation(-90.f, 0.f);
	object->addComponent<lunar::Camera>();

	lunar::UpdateCameras(scene, lunar::FrameTime{});

	const lunar::Camera* camera = object->getComponent<lunar::Camera>();
	EXPECT_NEAR(camera->front.x, 0.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(camera->front.z, -1.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(camera->right.x, 1.f, FLOAT_TOLERANCE);
	EXPECT_FLOAT_EQ(camera->position.x, 1.f);
	EXPECT_FLOAT_EQ(camera->position.y, 5.f);

	const glm::vec4 origin_in_view = camera->getViewMatrix() * glm::vec4(camera->position, 1.f);
	EXPECT_NEAR(origin_in_view.x, 0.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(origin_in_view.y, 0.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(origin_in_view.z, 0.f, FLOAT_TOLERANCE);
}

TEST(GameObject, WorldPositionUpdatesWhenParentMovesAfterQuery)
{
	lunar::Scene scene;

	lunar::GameObject root  = scene.createGameObject("Root");
	lunar::GameObject child = scene.createGameObject("Child", root);
	child->getTransform().position = { 1.f, 0.f, 0.f };

	EXPECT_FLOAT_EQ(child->getWorldPos().x, 1.f);

	root->getTransform().position = { 5.f, 0.f, 0.f };

	EXPECT_FLOAT_EQ(child->getWorldPos().x, 6.f);
}

TEST(GameObject, WorldPositionUpdatesWhenTransformIsWrittenDirectly)
{
	lunar::Scene scene;

	lunar::GameObject object = scene.createGameObject("Object");
	EXPECT_FLOAT_EQ(object->getWorldPos().y, 0.f);

	scene.forEach<lunar::Transform>([](lunar::Entity, lunar::Transform& transform) {
		transform.position.y = 3.f;
	});

	EXPECT_FLOAT_EQ(object->getWorldPos().y, 3.f);
}

TEST(GameObject, WorldRotationAndScaleCombineParents)
{
	lunar::Scene scene;

	lunar::GameObject root  = scene.createGameObject("Root");
	lunar::GameObject child = scene.createGameObject("Child", root);
	root->getTransform().rotation  = glm::angleAxis(glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
	root->getTransform().scale     = { 2.f, 2.f, 2.f };
	child->getTransform().position = { 1.f, 0.f, 0.f };
	child->getTransform().scale    = { 3.f, 3.f, 3.f };

	const glm::vec3 world_position = child->getWorldPos();
	const glm::quat world_rotation = child->getWorldRotation();
	const glm::quat root_rotation  = root->getWorldRotation();

	EXPECT_NEAR(world_position.x, 0.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(world_position.z, -2.f, FLOAT_TOLERANCE);
	EXPECT_NEAR(world_rotation.w, root_rotation.w, FLOAT_TOLERANCE);
	EXPECT_NEAR(world_rotation.y, root_rotation.y, FLOAT_TOLERANCE);
	EXPECT_FLOAT_EQ(child->getWorldScale().x, 6.f);
}

TEST(GameObject, DestroyIsDeferredUntilFlush)
{
	lunar::Scene scene;

	lunar::GameObject object = scene.createGameObject("Object");
	object->destroy();

	EXPECT_TRUE(object.valid());

	scene.flushDestroyedEntities();

	EXPECT_TRUE(object == nullptr);
	EXPECT_TRUE(scene.getGameObject("Object") == nullptr);
}

TEST(GameObject, DestroyRemovesWholeSubtree)
{
	lunar::Scene scene;

	lunar::GameObject root       = scene.createGameObject("Root");
	lunar::GameObject child      = scene.createGameObject("Child", root);
	lunar::GameObject grandchild = scene.createGameObject("Grandchild", child);
	lunar::GameObject unrelated  = scene.createGameObject("Unrelated");

	root->destroy();
	scene.flushDestroyedEntities();

	EXPECT_TRUE(root == nullptr);
	EXPECT_TRUE(child == nullptr);
	EXPECT_TRUE(grandchild == nullptr);
	EXPECT_TRUE(unrelated.valid());
}

TEST(GameObject, DestroyUnlinksFromParentAndKeepsSiblings)
{
	lunar::Scene scene;

	lunar::GameObject root   = scene.createGameObject("Root");
	lunar::GameObject first  = scene.createGameObject("First", root);
	lunar::GameObject middle = scene.createGameObject("Middle", root);
	lunar::GameObject last   = scene.createGameObject("Last", root);

	middle->destroy();
	scene.flushDestroyedEntities();

	auto children = root->getChildren();
	ASSERT_EQ(children.size(), 2u);
	EXPECT_EQ(children[0], first);
	EXPECT_EQ(children[1], last);

	first->destroy();
	scene.flushDestroyedEntities();

	children = root->getChildren();
	ASSERT_EQ(children.size(), 1u);
	EXPECT_EQ(children[0], last);
}

TEST(GameObject, DestroyFiresDeletedEventForEveryObjectInSubtree)
{
	lunar::Scene scene;
	int          deleted_count = 0;

	scene.addEventListener<lunar::Events::SceneObjectDeleted>([&](lunar::Events::SceneObjectDeleted& event) {
		EXPECT_TRUE(event.gameObject.valid());
		deleted_count++;
	});

	lunar::GameObject root = scene.createGameObject("Root");
	scene.createGameObject("Child", root);
	scene.createGameObject("Other child", root);

	root->destroy();
	scene.flushDestroyedEntities();

	EXPECT_EQ(deleted_count, 3);
}

TEST(GameObject, SetParentMovesObjectBetweenParents)
{
	lunar::Scene scene;

	lunar::GameObject first_parent  = scene.createGameObject("First parent");
	lunar::GameObject second_parent = scene.createGameObject("Second parent");
	lunar::GameObject child         = scene.createGameObject("Child", first_parent);
	second_parent->getTransform().position = { 0.f, 10.f, 0.f };

	child->setParent(second_parent);

	EXPECT_EQ(child->getParent(), second_parent);
	EXPECT_TRUE(first_parent->getChildren().empty());
	ASSERT_EQ(second_parent->getChildren().size(), 1u);
	EXPECT_FLOAT_EQ(child->getWorldPos().y, 10.f);
}

TEST(GameObject, SetParentToNullMakesObjectRoot)
{
	lunar::Scene scene;

	lunar::GameObject parent = scene.createGameObject("Parent");
	lunar::GameObject child  = scene.createGameObject("Child", parent);
	parent->getTransform().position = { 7.f, 0.f, 0.f };

	child->setParent(nullptr);

	EXPECT_TRUE(child->getParent() == nullptr);
	EXPECT_TRUE(parent->getChildren().empty());
	EXPECT_FLOAT_EQ(child->getWorldPos().x, 0.f);
}
