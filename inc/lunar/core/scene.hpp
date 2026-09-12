#pragma once
#include <lunar/core/gameobject.hpp>
#include <lunar/core/scene_event.hpp>
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
		void                    setMainCamera(Camera* camera);
		std::string_view        getName() const;
		void                    setName(const std::string_view& name);
		GameObject              getGameObject(size_t number);
		GameObject              getGameObject(const std::string_view& name);
		std::span<GameObject_T> getGameObjects();
		std::span<Component>    getComponents();
		GameObject              createGameObject
		(
			const std::string_view& name,
			GameObject_T*           parent = nullptr
		);

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
		std::string          name         = "Scene";
		vector<GameObject_T> objects      = {};
		vector<Component>    components   = {};
		rp3d::PhysicsWorld*  physicsWorld = nullptr;
		Camera*              mainCamera   = nullptr;

		inline void fireEvent(SceneEventType type, Event& e)
		{
			EventHandler::fireEvent((size_t)type, e);
		}

		friend class GameObject_T;
	};

	struct LUNAR_API SceneLoader
	{
		using ComponentJsonParser = std::function<Component(const nlohmann::json&)>;
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
			return useCustomClassSerializer(componentName, [](const nlohmann::json& json) -> Component {
				auto component = std::make_shared<T>(T::Deserialize(json));
				return component;
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
