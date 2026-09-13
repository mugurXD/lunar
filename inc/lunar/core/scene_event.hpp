#pragma once
#include <lunar/api.hpp>
#include <lunar/core/event.hpp>
#include <lunar/core/gameobject.hpp>
#include <functional>

namespace lunar
{
	enum class LUNAR_API SceneEventType : size_t
	{
		eUnknown       = 0,
		eObjectDeleted = 1,
		eObjectCreated = 2,
	};

	struct LUNAR_API SceneEvent : Event 
	{
	public:
		SceneEvent(Scene& sc) : scene(sc) {}

		Scene& scene;
	};

	template<typename T>
	concept SceneEventClass = 
		std::derived_from<T, SceneEvent> &&
		requires(T t) {
			{ T::getType() } -> std::same_as<SceneEventType>;
		};

	namespace Events
	{
		/*
			This event is triggered right before a GameObject is deleted.
			@param scene - reference to the scene that triggered this event
			@param gameObject - reference to the object that will be deleted
		*/
		struct LUNAR_API SceneObjectDeleted : public SceneEvent
		{
		public:
			SceneObjectDeleted(Scene& sc, GameObject& obj) : 
				SceneEvent(sc),
				gameObject(obj)
			{}

			GameObject gameObject;

			static SceneEventType getType() { return SceneEventType::eObjectDeleted; }
		};

		/*
			This event is triggered after a GameObject has been created.
			@param scene - reference to the scene that triggered this event
			@param gameObject - reference to the object created
		*/
		struct LUNAR_API SceneObjectCreated : public SceneEvent
		{
		public:
			SceneObjectCreated(Scene& sc, GameObject& obj) : 
				SceneEvent(sc), 
				gameObject(obj)
			{}

			GameObject gameObject;

			static SceneEventType getType() { return SceneEventType::eObjectCreated; }
		};
	}

}
