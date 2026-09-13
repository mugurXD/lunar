#pragma once
#include <lunar/api.hpp>
#include <lunar/core/event.hpp>
#include <lunar/core/gameobject.hpp>
#include <functional>

namespace lunar
{
	enum class LUNAR_API GObjectEventType : size_t
	{
		eUnknown        = 0,
	};

	struct LUNAR_API GameObjectEvent : Event
	{
	public:
		GameObjectEvent(GameObject obj) : gameObject(obj) {}

		GameObject gameObject;
	};

	template<typename T>
	concept GObjectEventClass =
		std::derived_from<T, GameObjectEvent> &&
		requires(T t) {
			{ T::getType() } -> std::same_as<GObjectEventType>;
		};

	namespace Events
	{
		
	}
}