#pragma once
#include <lunar/api.hpp>
#include <lunar/core/common.hpp>
#include <lunar/core/gameobject.hpp>
#include <lunar/core/handle.hpp>
#include <lunar/utils/identifiable.hpp>
#include <glm/glm.hpp>
#include <string_view>
#include <string>

namespace Render { class LUNAR_API RenderContext; }

namespace lunar
{
	class LUNAR_API Component_T
	{
	public:
		Component_T(GameObject parent) noexcept;
		Component_T()                  noexcept = default;
		virtual ~Component_T()         noexcept = default;

		virtual void     start()                {};
		virtual void     update()               {};
		const char*      getClassName()  const;
		const GameObject getGameObject() const;
		GameObject       getGameObject();
		Scene*           getScene();
		const Scene*     getScene()      const;
		Transform&       getTransform();
		const Transform& getTransform()  const;

	protected:
		GameObject gameObject = nullptr;
		Scene*   scene      = nullptr;

		friend class GameObject;
	};
	
	template<typename T>
	concept IsComponentType = std::derived_from<T, Component_T> && !std::is_same_v<T, Component_T>;
}

//namespace Core
//{
//	enum class LUNAR_API ComponentUpdatePriority
//	{
//		eNormal = 0,
//		eLow = 1,
//		eHigh = 2,
//	};
//
//	enum class LUNAR_API ComponentClassFlagBits : uint64_t
//	{
//		eNone       = 0,
//		eUpdateable = 1 << 0,
//		eRenderable = 1 << 1,
//		eUiDrawable = 1 << 2
//	};
//
//	using ComponentClassFlags = uint64_t;
//
//	class LUNAR_API Scene;
//	class LUNAR_API GameObject;
//	class LUNAR_API TransformComponent;
//
//	class LUNAR_API Component
//	{
//	public:
//        Component() = default;
//
//		virtual void                    update()                             {}
//		virtual void                    renderUpdate(Render::RenderContext&) {}
//		virtual void                    drawDebugUI(Render::RenderContext&);
//
//		Scene&                          getScene();
//		const Scene&                    getScene() const;
//		GameObject&                     getGameObject();
//		const GameObject&               getGameObject() const;
//		TransformComponent&             getTransform();
//		const TransformComponent&       getTransform() const;
//
//		virtual ComponentUpdatePriority _getClassPriority() { return ComponentUpdatePriority::eNormal; }
//		virtual ComponentClassFlags     _getClassFlags()    { return {}; }
//		virtual const char*             _getClassName()     { return typeid(*this).name(); }
//
//	protected:
//		Scene*                   _scene      = nullptr;
//		Identifiable::NativeType _gameObject = -1;
//		friend class GameObject;
//	};
//
//	class LUNAR_API TransformComponent : public Component
//	{
//	public:
//		glm::vec3 position = { 0, 0, 0 };
//		glm::vec3 rotation = { 0, 0, 0 };
//		glm::vec3 scale    = { 1, 1, 1 };
//	};
//
//	template<typename T>
//	concept IsDerivedComponent = std::derived_from<T, Component> && !std::is_same_v<T, Component>;
//
//	inline ComponentClassFlags operator|(ComponentClassFlagBits a, ComponentClassFlagBits b)
//	{
//		return static_cast<ComponentClassFlags>(
//			static_cast<uint64_t>(a) | static_cast<uint64_t>(b)
//		);
//	}
//
//	inline ComponentClassFlags operator|(ComponentClassFlags a, ComponentClassFlagBits b)
//	{
//		return static_cast<ComponentClassFlags>(
//			static_cast<uint64_t>(a) | static_cast<uint64_t>(b)
//		);
//	}
//
//	inline bool operator&(ComponentClassFlags a, ComponentClassFlagBits b)
//	{
//		return static_cast<uint64_t>(a) & static_cast<uint64_t>(b);
//	}
//}
