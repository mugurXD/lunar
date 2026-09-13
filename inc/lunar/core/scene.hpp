#pragma once
#include <lunar/core/handle.hpp>
#include <lunar/core/component_storage.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/core/component.hpp>
#include <lunar/core/scene_event.hpp>
#include <lunar/core/system.hpp>
#include <lunar/core/event.hpp>
#include <lunar/render/common.hpp>
#include <lunar/file/json_file.hpp>
#include <lunar/utils/collections.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>

#include <reactphysics3d/reactphysics3d.h>

namespace Render { class LUNAR_API Camera; }

namespace lunar
{
	using PhysicsWorld = reactphysics3d::PhysicsWorld;

	class LUNAR_API Camera;
	class LUNAR_API Scene : public EventHandler
	{
	public:
		Scene(const std::string_view& name) noexcept;

		Scene(Scene&&)                      noexcept = delete;
		Scene(const Scene&)                 noexcept = delete;
		
		Scene()                             noexcept;
		~Scene()                            noexcept;

		void                    update();
		void                    physicsUpdate(double dt);
		PhysicsWorld*           getPhysicsWorld();
		Camera*                 getMainCamera();
		void                    setMainCamera(GameObject camera_object);
		Render::GpuCubemap      getEnvironment() const;
		void                    setEnvironment(Render::GpuCubemap environment_map);
		void                    updateBehaviours(const FrameTime& frame_time);
		std::string_view        getName() const;
		void                    setName(const std::string_view& name);
		GameObject              getGameObject(const std::string_view& name);
		//std::span<GameObject_T> getGameObjects();
		//std::span<Component>    getComponents();
		GameObject              createGameObject
		(
			const std::string_view& name,
			GameObject              parent = nullptr
		);

		Entity                  createEntity();
		void                    destroyEntity(const Entity& entity);
		void                    flushDestroyedEntities();
		void                    setParent(const Entity& child, const Entity& parent);
		const WorldTransform&   resolveWorldTransform(const Entity& entity);

		template<typename T, typename... Args>
		T& addComponent(const Entity& entity, Args&&... args)
		{
			EntityRecord* record = entities.get(entity);
			DEBUG_ASSERT(record != nullptr, "Entity is stale or belongs to another scene");

			T& component = getOrCreateStorage<T>().add(entity, std::forward<Args>(args)...);
			record->components.set(GetComponentTypeId<T>());
			return component;
		}

		template<typename T>
		T* getComponent(const Entity& entity)
		{
			ComponentStorage<T>* storage = findStorage<T>();
			return storage == nullptr ? nullptr : storage->get(entity);
		}

		template<typename T>
		bool hasComponent(const Entity& entity) const
		{
			const ComponentStorage<T>* storage = findStorage<T>();
			return storage != nullptr && storage->has(entity);
		}

		template<typename T>
		void removeComponent(const Entity& entity)
		{
			EntityRecord*        record  = entities.get(entity);
			ComponentStorage<T>* storage = findStorage<T>();
			if (record == nullptr || storage == nullptr)
				return;

			storage->remove(entity);
			record->components.reset(GetComponentTypeId<T>());
		}

		template<typename... Ts, typename Function>
		void forEach(Function&& function)
		{
			const ComponentStorageBase* smallest = findSmallestStorage<Ts...>();
			if (smallest == nullptr)
				return;

			const vector<Entity>& candidates = smallest->getEntities();
			for (size_t position = 0; position < candidates.size(); position++)
			{
				const Entity entity = candidates[position];
				if ((findStorage<Ts>()->has(entity) && ...))
					function(entity, *findStorage<Ts>()->get(entity)...);
			}
		}

		template<SceneEventClass T>
		inline void             addEventListener(EventListener_T<T> listener)
		{
			EventHandler::addEventListener((size_t)T::getType(), [listener](Event& e) {
				listener(static_cast<T&>(e)); 
			});
		}

		/*
			Due to how object handles currently work, copying or moving
			the scene object would invalidate all of them, so it's probably
			a better idea to just disable this functionality. 
			I don't think it would be a great idea to copy/move this object
			in any case \(o_o)/
		*/

		Scene& operator=(const Scene&) = delete;
		Scene& operator=(Scene&&)      = delete;

	private:
		std::string         name         = "Scene";
		rp3d::PhysicsWorld* physicsWorld = nullptr;
		GameObject          mainCamera   = nullptr;
		Render::GpuCubemap  environment  = nullptr;

		Pool<EntityRecord>                            entities          = {};
		vector<std::unique_ptr<ComponentStorageBase>> componentStorages = {};
		vector<Entity>                                destroyedEntities = {};
		uint64_t                                      transformVersion  = 0;
		vector<System>                                behaviourUpdaters = {};
		ComponentMask                                 behaviourTypes    = {};

		template<typename T>
		void registerBehaviourType()
		{
			const size_t type_id = GetComponentTypeId<T>();
			if (behaviourTypes.test(type_id))
				return;

			behaviourTypes.set(type_id);
			behaviourUpdaters.push_back([](Scene& scene, const FrameTime& frame_time) {
				scene.forEach<T>([&](Entity, T& behaviour) { behaviour.update(frame_time); });
			});
		}

		friend class GameObject;

		inline void fireEvent(SceneEventType type, Event& e)
		{
			EventHandler::fireEvent((size_t)type, e);
		}

		void attachChild(const Entity& parent, const Entity& child);
		void detachFromParent(const Entity& child);
		void prepareGameObjectDestruction(const Entity& entity);

		template<typename T>
		ComponentStorage<T>* findStorage() const
		{
			const size_t type_id = GetComponentTypeId<T>();
			if (type_id >= componentStorages.size())
				return nullptr;

			return static_cast<ComponentStorage<T>*>(componentStorages[type_id].get());
		}

		template<typename T>
		ComponentStorage<T>& getOrCreateStorage()
		{
			const size_t type_id = GetComponentTypeId<T>();
			if (type_id >= componentStorages.size())
				componentStorages.resize(type_id + 1);

			std::unique_ptr<ComponentStorageBase>& storage = componentStorages[type_id];
			if (storage == nullptr)
				storage = std::make_unique<ComponentStorage<T>>();

			return static_cast<ComponentStorage<T>&>(*storage);
		}

		template<typename... Ts>
		const ComponentStorageBase* findSmallestStorage() const
		{
			const ComponentStorageBase* smallest = nullptr;
			for (const ComponentStorageBase* storage : { static_cast<const ComponentStorageBase*>(findStorage<Ts>())... })
			{
				if (storage == nullptr)
					return nullptr;

				if (smallest == nullptr || storage->size() < smallest->size())
					smallest = storage;
			}

			return smallest;
		}
	};

	template<typename T>
	T* GameObject::getComponent()
	{
		return scene->getComponent<T>(entity);
	}

	template<typename T, typename... Args>
	T* GameObject::addComponent(Args&&... args)
	{
		T& component = scene->addComponent<T>(entity, std::forward<Args>(args)...);

		if constexpr (std::derived_from<T, Component_T>)
		{
			component.gameObject = *this;
			component.scene      = scene;
			scene->registerBehaviourType<T>();
			component.start();
		}

		return &component;
	}

	struct LUNAR_API SceneLoader
	{
		using ComponentJsonParser = std::function<void(GameObject, const nlohmann::json&)>;
		using VisitorDict         = std::unordered_map<std::string, ComponentJsonParser>;


		SceneLoader()  noexcept = default;
		~SceneLoader() noexcept  = default;
	
		SceneLoader& destination(Scene& scene);
		SceneLoader& useRenderContext(Render::RenderContext context);
		SceneLoader& loadJsonFile(const Fs::Path& path);

		SceneLoader& useCoreSerializers();
		SceneLoader& useCustomClassSerializer(
			const std::string& componentName,
			const ComponentJsonParser& parser
		);

		template<typename T> requires IsComponentType<T> && IsJsonSerializable<T>
		SceneLoader& useClassSerializer(const std::string& componentName)
		{
			return useCustomClassSerializer(componentName, [](GameObject object, const nlohmann::json& json) {
				object.addComponent<T>(T::Deserialize(json));
			});
		}

	private:
		void parseComponents(GameObject object, const nlohmann::json& json);
		void parseTransform(GameObject object, const nlohmann::json& json);
		void parseGameObject(
			const nlohmann::json& json, 
			GameObject            parent = nullptr
		);

		VisitorDict           visitors      = {};
		Scene*                result        = nullptr;
		Render::RenderContext renderContext = nullptr;
	};
}

namespace Core
{
	//class LUNAR_API Scene : public EventHandler
	//{
	//public:
	//	Scene(
	//		const std::string& name
	//	);
	//	Scene() = default;
	//	Scene(Scene&&)            = delete;
	//	Scene& operator=(Scene&&) = delete;

	//	void                     update();
	//	void                     renderUpdate(Render::RenderContext& context);
 //       const std::string&       getName() const;
 //       size_t                   getNameHash() const;
 //       GameObject&              getGameObject(const std::string_view& name);
	//	const GameObject&        getGameObject(const std::string_view& name) const;
 //       GameObject&              getGameObject(Identifiable::NativeType id);
	//	const GameObject&        getGameObject(Identifiable::NativeType id) const;
	//	std::vector<GameObject>& getGameObjects();
	//	GameObject&              createGameObject(const std::string_view& name, GameObject* parent = nullptr);
	//	void                     deleteGameObject(Identifiable::NativeType id);
	//	Render::Camera*          getMainCamera();
	//	void                     setMainCamera(Render::Camera& camera);
	//	void                     addEventListener(SceneEventType type, EventListener listener);
	//	void                     removeEventListener(SceneEventType type, EventListener listener);

	//	template<typename T>
	//	inline void              addEventListener(EventListener_T<T> listener)
	//	{
	//		auto type = imp::GetSceneEventType<T>();
	//		addEventListener(type, [listener](Core::Event& e) { listener(static_cast<T&>(e)); });
	//	}

	//private:
	//	void triggerEvent(SceneEventType type, Event& e);

	//	Identifiable::NativeType                 mainCamera    = -1;
	//	size_t                                   nameHash      = SIZE_MAX;
	//	std::string                              name          = "Untitled Scene";
	//	std::vector<GameObject>                  objects       = {};
	//	rp3d::PhysicsCommon                      physicsCommon = {};
	//	rp3d::PhysicsWorld*                      physicsWorld  = nullptr;
	//};


}
