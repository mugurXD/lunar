#include <lunar/core/scene.hpp>
#include <lunar/core/time.hpp>
#include <lunar/debug/log.hpp>
#include <lunar/render/components.hpp>

#include <reactphysics3d/reactphysics3d.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lunar
{
	reactphysics3d::PhysicsCommon PHYSICS_COMMON;

	Scene::Scene(const std::string_view& name) noexcept
		: name(name),
		physicsWorld(PHYSICS_COMMON.createPhysicsWorld())
	{

	}

	Scene::Scene() noexcept
		: physicsWorld(PHYSICS_COMMON.createPhysicsWorld())
	{
	}

	Scene::~Scene() noexcept
	{
	}

	GameObject Scene::getGameObject(const std::string_view& name)
	{
		ComponentStorage<Name>* names = findStorage<Name>();
		if (names == nullptr)
			return nullptr;

		for (const Entity& entity : names->getEntities())
			if (names->get(entity)->value == name)
				return GameObject(this, entity);

		return nullptr;
	}

	GameObject Scene::createGameObject(const std::string_view& name, GameObject parent)
	{
		DEBUG_ASSERT(name.size() > 0);
		DEBUG_ASSERT(parent == nullptr || parent->getScene() == this);

		const Entity entity = createEntity();
		addComponent<Name>(entity, std::string(name));
		addComponent<Transform>(entity);
		addComponent<Hierarchy>(entity);
		addComponent<WorldTransform>(entity);

		if (parent != nullptr)
			attachChild(parent.getEntity(), entity);

		GameObject object = GameObject(this, entity);
		auto       event  = Events::SceneObjectCreated(*this, object);

		fireEvent(SceneEventType::eObjectCreated, event);

		return object;
	}

	void Scene::attachChild(const Entity& parent, const Entity& child)
	{
		getComponent<Hierarchy>(child)->parent = parent;

		Hierarchy* parent_hierarchy = getComponent<Hierarchy>(parent);
		if (parent_hierarchy->firstChild == nullptr)
		{
			parent_hierarchy->firstChild = child;
			return;
		}

		Entity last_child = parent_hierarchy->firstChild;
		while (getComponent<Hierarchy>(last_child)->nextSibling != nullptr)
			last_child = getComponent<Hierarchy>(last_child)->nextSibling;

		getComponent<Hierarchy>(last_child)->nextSibling = child;
	}

	void Scene::detachFromParent(const Entity& child)
	{
		Hierarchy*   child_hierarchy = getComponent<Hierarchy>(child);
		const Entity parent          = child_hierarchy->parent;
		const Entity next_sibling    = child_hierarchy->nextSibling;

		child_hierarchy->parent      = nullptr;
		child_hierarchy->nextSibling = nullptr;

		if (parent == nullptr)
			return;

		Hierarchy* parent_hierarchy = getComponent<Hierarchy>(parent);
		if (parent_hierarchy->firstChild == child)
		{
			parent_hierarchy->firstChild = next_sibling;
			return;
		}

		Entity previous_sibling = parent_hierarchy->firstChild;
		while (previous_sibling != nullptr && getComponent<Hierarchy>(previous_sibling)->nextSibling != child)
			previous_sibling = getComponent<Hierarchy>(previous_sibling)->nextSibling;

		if (previous_sibling != nullptr)
			getComponent<Hierarchy>(previous_sibling)->nextSibling = next_sibling;
	}

	void Scene::setParent(const Entity& child, const Entity& parent)
	{
		for (Entity ancestor = parent; ancestor != nullptr; ancestor = getComponent<Hierarchy>(ancestor)->parent)
			DEBUG_ASSERT(ancestor != child, "Cannot parent an object to itself or to one of its descendants");

		detachFromParent(child);

		if (parent != nullptr)
			attachChild(parent, child);
	}

	const WorldTransform& Scene::resolveWorldTransform(const Entity& entity)
	{
		WorldTransform*  world     = getComponent<WorldTransform>(entity);
		const Transform* local     = getComponent<Transform>(entity);
		const Hierarchy* hierarchy = getComponent<Hierarchy>(entity);
		DEBUG_ASSERT(world != nullptr && local != nullptr && hierarchy != nullptr, "Entity is missing its transform components");

		const WorldTransform* parent_world   = hierarchy->parent == nullptr ? nullptr : &resolveWorldTransform(hierarchy->parent);
		const uint64_t        parent_version = parent_world == nullptr ? 0 : parent_world->version;

		const bool is_up_to_date = world->version != 0
			&& world->source == *local
			&& world->parent == hierarchy->parent
			&& world->parentVersion == parent_version;

		if (is_up_to_date)
			return *world;

		const glm::quat local_rotation = glm::quat(glm::radians(local->rotation));
		const glm::mat4 local_matrix   = glm::translate(glm::mat4(1.f), local->position)
			* glm::mat4(local_rotation)
			* glm::scale(glm::mat4(1.f), local->scale);

		world->matrix        = parent_world == nullptr ? local_matrix   : parent_world->matrix * local_matrix;
		world->rotation      = parent_world == nullptr ? local_rotation : glm::normalize(parent_world->rotation * local_rotation);
		world->scale         = parent_world == nullptr ? local->scale   : parent_world->scale * local->scale;
		world->source        = *local;
		world->parent        = hierarchy->parent;
		world->parentVersion = parent_version;
		world->version       = ++transformVersion;

		return *world;
	}

	Entity Scene::createEntity()
	{
		return entities.create();
	}

	void Scene::destroyEntity(const Entity& entity)
	{
		destroyedEntities.push_back(entity);
	}

	void Scene::flushDestroyedEntities()
	{
		for (size_t position = 0; position < destroyedEntities.size(); position++)
		{
			const Entity entity = destroyedEntities[position];
			if (!entities.contains(entity))
				continue;

			if (hasComponent<Hierarchy>(entity))
				prepareGameObjectDestruction(entity);

			const EntityRecord* record = entities.get(entity);
			for (size_t type_id = 0; type_id < componentStorages.size(); type_id++)
				if (record->components.test(type_id))
					componentStorages[type_id]->remove(entity);

			entities.destroy(entity);
		}

		destroyedEntities.clear();
	}

	void Scene::prepareGameObjectDestruction(const Entity& entity)
	{
		GameObject object = GameObject(this, entity);
		auto       event  = Events::SceneObjectDeleted(*this, object);

		fireEvent(SceneEventType::eObjectDeleted, event);

		for (Entity child = getComponent<Hierarchy>(entity)->firstChild; child != nullptr; child = getComponent<Hierarchy>(child)->nextSibling)
			destroyedEntities.push_back(child);

		detachFromParent(entity);
	}

	void Scene::setMainCamera(GameObject camera_object)
	{
		DEBUG_ASSERT(camera_object == nullptr || camera_object->getScene() == this, "Camera belongs to another scene");
		this->mainCamera = camera_object;
	}

	Camera* Scene::getMainCamera()
	{
		return mainCamera == nullptr ? nullptr : mainCamera.getComponent<Camera>();
	}

	Render::GpuCubemap Scene::getEnvironment() const
	{
		return environment;
	}

	void Scene::setEnvironment(Render::GpuCubemap environment_map)
	{
		this->environment = environment_map;
	}

	void Scene::updateBehaviours(const FrameTime& frame_time)
	{
		for (size_t position = 0; position < behaviourUpdaters.size(); position++)
		{
			const System updater = behaviourUpdaters[position];
			updater(*this, frame_time);
		}
	}

	std::string_view Scene::getName() const
	{
		return name;
	}

	void Scene::setName(const std::string_view& name)
	{
		this->name = name;
	}

	void Scene::physicsUpdate(double dt)
	{
		this->physicsWorld->update(dt);
	}

	PhysicsWorld* Scene::getPhysicsWorld()
	{
		return this->physicsWorld;
	}
}

//namespace Core
//{
//	SceneBuilder& SceneBuilder::setName(const std::string_view& name)
//	{
//		this->name = name;
//		return *this;
//	}
//

//
//	std::shared_ptr<Scene> SceneBuilder::create()
//	{
//		auto scene = std::make_shared<Scene>(name);
//
//		if (!std::filesystem::exists(jsonFile))
//			return scene;
//
//		auto file = Fs::JsonFile(jsonFile);
//		const auto& json = file.content;
//
//		name = json["name"];
//		if (!json.contains("gameObjects"))
//			return scene;
//
//		auto& game_objects = json["gameObjects"];
//		for (auto& [key, game_object] : game_objects.items())
//			parseGameObject(game_object, scene.get());
//
//		return scene;
//	}
//
//	Scene::Scene
//	(
//		const std::string& name
//	) : name(name),
//		nameHash(std::hash<std::string>{}(name)),
//		objects(),
//		Identifiable()
//	{
//		physicsWorld = physicsCommon.createPhysicsWorld();
//	}
//
//	const std::string& Scene::getName() const
//	{
//		return name;
//	}
//
//	size_t Scene::getNameHash() const
//	{
//		return nameHash;
//	}
//
//	void Scene::deleteGameObject(Identifiable::NativeType id)
//	{
//		// TODO: Rethink this function; it somehow messes up the ids of all objects
//		
//		auto& object       = getGameObject(id);
//
//		auto  delete_event = Events::SceneObjectDeleted(*this, object);
//		triggerEvent(SceneEventType::eObjectDeleted, delete_event);
//
//		auto  children = object.getChildren();
//		for (size_t i = 0; i < children.size(); i++)
//			deleteGameObject(children[i]->getId());
//
//		for (size_t i = 0; i < objects.size(); i++)
//		{
//			if (objects[i].getId() != id)
//				continue;
//
//			auto name = objects[i].getName();
//
//			std::iter_swap(objects.begin() + i, objects.end() - 1);
//			objects.pop_back();
//			
//			DEBUG_LOG("Deleted object '{}'", name);
//			return;
//		}
//
//		DEBUG_ASSERT(true, "deleteGameObject called on inexistent id");
//	}
//
//	GameObject& Scene::createGameObject(const std::string_view& name, GameObject* parent)
//	{
//		auto& created_object = objects.emplace_back(name, this, parent);
//		auto  created_event  = Events::SceneObjectCreated(*this, created_object);
//		triggerEvent(SceneEventType::eObjectCreated, created_event);
//		return created_object;
//	}
//
//    GameObject& Scene::getGameObject(Identifiable::NativeType id)
//    {
//        for(auto& game_object : objects)
//        {
//            if(game_object.getId() == id)
//                return game_object;
//        }
//
//        DEBUG_ERROR("Called on inexistent game object (id: {})", id);
//        throw;
//    }
//
//	const GameObject& Scene::getGameObject(Identifiable::NativeType id) const
//	{
//		for (auto& game_object : objects)
//		{
//			if (game_object.getId() == id)
//				return game_object;
//		}
//
//		DEBUG_ERROR("Called on inexistent game object (id: {})", id);
//		throw;
//	}
//
//    GameObject& Scene::getGameObject(const std::string_view& name)
//    {
//		size_t name_hash = Lunar::imp::fnv1a_hash(name);
//		for (auto& game_object : objects)
//		{
//			if (game_object.getNameHash() == name_hash)
//				return game_object;
//		}
//
//		DEBUG_ERROR("Called on inexistent game object (name: {})", name);
//        throw;
//    }
//
//	const GameObject& Scene::getGameObject(const std::string_view& name) const
//	{
//		size_t name_hash = Lunar::imp::fnv1a_hash(name);
//		for (auto& game_object : objects)
//		{
//			if (game_object.getNameHash() == name_hash)
//				return game_object;
//		}
//
//		DEBUG_ERROR("Called on inexistent game object (name: {})", name);
//		throw;
//	}
//
//	std::vector<GameObject>& Scene::getGameObjects()
//	{
//		return objects;
//	}
//
//	void Scene::setMainCamera(Render::Camera& camera)
//	{
//		mainCamera = camera
//			.getGameObject()
//			.getId();
//	}
//
//	Render::Camera* Scene::getMainCamera()
//	{
//		if (mainCamera == -1)
//			return nullptr;
//
//		return getGameObject(mainCamera)
//			.getComponent<Render::Camera>();
//	}
//
//	void Scene::update()
//	{
//		for (auto& object : getGameObjects())
//			object.update();
//
//		physicsWorld->update(Time::DeltaTime());
//	}
//
//	void Scene::renderUpdate(Render::RenderContext& context)
//	{
//		for (auto& object : getGameObjects())
//			object.renderUpdate(context);
//	}
//
//	void Scene::addEventListener(SceneEventType type, EventListener listener)
//	{
//		_addEventListener(static_cast<size_t>(type), listener);
//	}
//
//	void Scene::removeEventListener(SceneEventType type, EventListener listener)
//	{
//		_removeEventListener(static_cast<size_t>(type), listener);
//	}
//
//	void Scene::triggerEvent(SceneEventType type, Event& e)
//	{
//		_triggerEvent(static_cast<size_t>(type), e);
//	}
//}
