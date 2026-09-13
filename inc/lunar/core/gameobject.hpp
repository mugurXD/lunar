#pragma once
#include <lunar/core/common.hpp>
#include <lunar/core/component_storage.hpp>
#include <lunar/api.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lunar
{
	struct LUNAR_API Name
	{
		std::string value = {};
	};

	struct LUNAR_API Transform
	{
		glm::vec3 position   = { 0, 0, 0 };
		glm::vec3 rotation   = { 0, 0, 0 };
		glm::vec3 scale      = { 1, 1, 1 };
	};

	struct LUNAR_API Hierarchy
	{
		Entity parent      = nullptr;
		Entity firstChild  = nullptr;
		Entity nextSibling = nullptr;
	};

	class LUNAR_API GameObject
	{
	public:
		GameObject()                                   noexcept = default;
		GameObject(std::nullptr_t)                     noexcept {}
		GameObject(Scene* scene, const Entity& entity) noexcept;
		~GameObject()                                  noexcept = default;

		GameObject*             operator->()                        { return this; }
		const GameObject*       operator->()                  const { return this; }
		bool                    operator==(const GameObject&) const = default;
		bool                    operator==(std::nullptr_t)    const { return !valid(); }

		bool                    valid()             const;
		Entity                  getEntity()         const;
		Scene*                  getScene()          const;
		std::string_view        getName()           const;
		GameObject              getParent()         const;
		std::vector<GameObject> getChildren()       const;
		const Transform&        getTransform()      const;
		Transform&              getTransform();
		glm::vec3               getWorldPos()       const;
		glm::quat               getWorldRotation()  const;
		glm::vec3               getWorldScale()     const;
		glm::mat4               getWorldTransform() const;
		glm::vec3               getLocalPos()       const;
		glm::vec3               getLocalRotation()  const;
		glm::vec3               getLocalScale()     const;
		void                    setWorldPos(glm::vec3 pos);
		void                    setLocalPos(glm::vec3 pos);
		GameObject              createChildObject(const std::string_view& name);

		template<typename T>
		T*                      getComponent();

		template<typename T, typename... Args>
		T*                      addComponent(Args&&... args);

	private:
		Scene* scene  = nullptr;
		Entity entity = nullptr;
	};
}

//namespace Core
//{
//	// TODO: sort components based on nameHash so binary search can be done
//	class LUNAR_API Scene_T;
//
//
//	class LUNAR_API GameObject : public Identifiable
//	{
//	public:
//		GameObject(const std::string_view& name, Scene* scene, GameObject* parent = nullptr);
//		GameObject();
//		~GameObject();
//
//		void                      update();
//		void                      renderUpdate(Render::RenderContext& context);
//        size_t                    getNameHash() const;
//        const std::string&        getName() const;
//		TransformComponent&       getTransform();
//		const TransformComponent& getTransform() const;
//
//		std::vector<GameObject*>  getChildren();
//		GameObject*               getParent();
//		Identifiable::NativeType  getParentId() const;
//		Scene*                    getParentScene();
//		std::span<std::shared_ptr<Component>> getComponents();
//		void                      addComponent(std::shared_ptr<Component> constructed);
//
//		template<typename T> requires IsDerivedComponent<T>
//		T* getComponent()
//		{
//			return static_cast<T*>(getComponent(typeid(T)));
//		}
//
//		template<typename T> requires IsDerivedComponent<T>
//		const T* getComponent() const
//		{
//			return static_cast<const T*>(getComponent(typeid(T)));
//		}
//
//		template <typename T, class... _Valty> requires IsDerivedComponent<T>
//		T& addComponent(_Valty&&... ctor_values)
//		{	
//			DEBUG_ASSERT(getComponent<T>() == nullptr, "There can exist only one component of type <T> on a single gameobject.");
//			std::shared_ptr<T> new_component = std::make_shared<T>(std::forward<_Valty>(ctor_values)...);
//			addComponent(new_component);
//			return *new_component;
//		}
//		
//	private:
//		Component* getComponent(const std::type_info& ty);
//
//        std::string name;
//		size_t nameHash;
//
//		TransformComponent transform;
//		std::vector<std::shared_ptr<Component>> components;
//
//		Scene* scene;
//		Identifiable::NativeType parent;	
//		
//		friend class Scene;
//	};
//}
