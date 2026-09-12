#include <lunar/core/scene.hpp>
#include <lunar/core/time.hpp>
#include <lunar/debug/log.hpp>
#include <lunar/render/components.hpp>

#include <reactphysics3d/reactphysics3d.h>

namespace lunar
{
	LUNAR_HANDLE_IMPL(GameObject);

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

	GameObject Scene::getGameObject(size_t number)
	{
		return make_handle(objects, number);
	}

	GameObject Scene::getGameObject(const std::string_view& name)
	{
		for (size_t i = 0; i < objects.size(); i++)
			if (objects[i].getName().compare(name) == 0)
				return make_handle(objects, i);

		return nullptr;
	}

	GameObject Scene::createGameObject(const std::string_view& name, GameObject_T* parent)
	{
		DEBUG_ASSERT(name.size() > 0);
		DEBUG_ASSERT(parent == nullptr || parent->getScene() == this);

		objects.emplace_back(this, name, parent == nullptr ? nullptr : make_handle(objects, parent));
		
		GameObject handle = make_handle(objects);
		auto       event  = Events::SceneObjectCreated(*this, handle);
		
		fireEvent(SceneEventType::eObjectCreated, event);

		return make_handle(objects);
	}

	std::span<GameObject_T> Scene::getGameObjects()
	{
		return objects;
	}

	std::span<Component> Scene::getComponents()
	{
		return components;
	}

	void Scene::setMainCamera(Camera* camera)
	{
		this->mainCamera = camera;
	}

	Camera* Scene::getMainCamera()
	{
		return mainCamera;
	}

	std::string_view Scene::getName() const
	{
		return name;
	}

	void Scene::setName(const std::string_view& name)
	{
		this->name = name;
	}

	void Scene::update()
	{
		for (auto& component : components)
			component->update();
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
