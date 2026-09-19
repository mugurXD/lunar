#pragma once
#include <lunar/api.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <initializer_list>
#include <unordered_map>
#include <string_view>
#include <vector>

namespace lunar
{
	namespace imp
	{
		enum class LUNAR_API ActionType
		{
			eNone    = 0,
			eKey     = 1 << 0,
			eMouse   = 1 << 1,
			eGamepad = 1 << 2,
		};

		struct LUNAR_API ActionData
		{
			ActionType type;
			size_t     value;
		};
	}

	/*
		Represents a list of actions that need to happen in order for the combo to be satisfied.
		Example: { "keyboard.esc", "mouse.left_button" } would require both to be pressed at the same time.
		Check "input.cpp" for a list of all supported values.
	*/
	using InputCombo = std::initializer_list<std::string_view>;

	class LUNAR_API InputHandler
	{
	public:	
		virtual void      update()            = 0;
		virtual glm::vec2 getAxis()     const = 0;
		virtual glm::vec2 getRotation() const = 0;
		virtual float     getScroll()   const = 0;
		virtual bool      getAction(const std::string_view&) const = 0;
		virtual bool      getActionUp(const std::string_view&) const = 0;
		virtual bool      getActionDown(const std::string_view&) const = 0;

		/*
			@brief Registers a new input action that can be checked.
			@param name Name of the action to be registered.
			@param combos A list of input combinations that can trigger this action.
			@example window->registerAction("close", { { "keyboard.esc" }, { "keyboard.alt", "keyboard.f4" } });
			@see input.cpp
		*/
		void              registerAction(const std::string_view& name, const std::initializer_list<InputCombo>& combos);

	protected:
		struct InputComboValues 
		{ 
			bool active = false; 
			imp::ActionData* key[4] = { nullptr, nullptr, nullptr, nullptr }; 
		};

		struct InputKeys 
		{ 
			InputComboValues combos[4] = { {}, {}, {}, {} };
		};

		std::unordered_map<std::string_view, InputKeys> actions          = {};
		float                                           deadzoneLeft     = 0.f;
		float                                           deadzoneRight    = 0.f;
		float                                           mouseSensitivity = 1.f;
	};

	enum class LUNAR_API KeyState : int
	{
		eNone     = 0,
		eReleased = 1 << 0,
		eHeld     = 1 << 1,
		ePressed  = 1 << 2,
	};

	inline bool     operator&(KeyState a, KeyState b) { return (bool)(static_cast<int>(a) & static_cast<int>(b));     }
	inline KeyState operator|(KeyState a, KeyState b) { return (KeyState)(static_cast<int>(a) | static_cast<int>(b)); }
}

namespace lunar::Input
{
	LUNAR_API InputHandler& GetGlobalHandler();
	LUNAR_API void          SetGlobalHandler(InputHandler*);
	LUNAR_API void          SetGlobalHandler(InputHandler&);
	LUNAR_API bool          GetAction(const std::string_view& name);
	LUNAR_API bool          GetActionUp(const std::string_view& name);
	LUNAR_API bool          GetActionDown(const std::string_view& name);
	LUNAR_API glm::vec2     GetAxis();
	LUNAR_API glm::vec2     GetRotation();
	LUNAR_API float         GetScroll();
}
