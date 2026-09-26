#include <lunar/core/input.hpp>
#include <lunar/debug/assert.hpp>

namespace lunar
{
	namespace imp
	{
		inline std::unordered_map<std::string_view, ActionData> Actions =
		{
			{ "keyboard.q",         ActionData { ActionType::eKey, GLFW_KEY_Q } },
			{ "keyboard.w",         ActionData { ActionType::eKey, GLFW_KEY_W } },
			{ "keyboard.e",         ActionData { ActionType::eKey, GLFW_KEY_E } },
			{ "keyboard.r",         ActionData { ActionType::eKey, GLFW_KEY_R } },
			{ "keyboard.t",         ActionData { ActionType::eKey, GLFW_KEY_T } },
			{ "keyboard.y",         ActionData { ActionType::eKey, GLFW_KEY_Y } },
			{ "keyboard.u",         ActionData { ActionType::eKey, GLFW_KEY_U } },
			{ "keyboard.i",         ActionData { ActionType::eKey, GLFW_KEY_I } },
			{ "keyboard.o",         ActionData { ActionType::eKey, GLFW_KEY_O } },
			{ "keyboard.p",         ActionData { ActionType::eKey, GLFW_KEY_P } },
			{ "keyboard.a",         ActionData { ActionType::eKey, GLFW_KEY_A } },
			{ "keyboard.s",         ActionData { ActionType::eKey, GLFW_KEY_S } },
			{ "keyboard.d",         ActionData { ActionType::eKey, GLFW_KEY_D } },
			{ "keyboard.f",         ActionData { ActionType::eKey, GLFW_KEY_F } },
			{ "keyboard.g",         ActionData { ActionType::eKey, GLFW_KEY_G } },
			{ "keyboard.h",         ActionData { ActionType::eKey, GLFW_KEY_H } },
			{ "keyboard.j",         ActionData { ActionType::eKey, GLFW_KEY_J } },
			{ "keyboard.k",         ActionData { ActionType::eKey, GLFW_KEY_K } },
			{ "keyboard.l",         ActionData { ActionType::eKey, GLFW_KEY_L } },
			{ "keyboard.z",         ActionData { ActionType::eKey, GLFW_KEY_Z } },
			{ "keyboard.x",         ActionData { ActionType::eKey, GLFW_KEY_X } },
			{ "keyboard.c",         ActionData { ActionType::eKey, GLFW_KEY_C } },
			{ "keyboard.v",         ActionData { ActionType::eKey, GLFW_KEY_V } },
			{ "keyboard.b",         ActionData { ActionType::eKey, GLFW_KEY_B } },
			{ "keyboard.n",         ActionData { ActionType::eKey, GLFW_KEY_N } },
			{ "keyboard.m",         ActionData { ActionType::eKey, GLFW_KEY_M } },
			{ "keyboard.1",         ActionData { ActionType::eKey, GLFW_KEY_1 } },
			{ "keyboard.2",         ActionData { ActionType::eKey, GLFW_KEY_2 } },
			{ "keyboard.3",         ActionData { ActionType::eKey, GLFW_KEY_3 } },
			{ "keyboard.4",         ActionData { ActionType::eKey, GLFW_KEY_4 } },
			{ "keyboard.5",         ActionData { ActionType::eKey, GLFW_KEY_5 } },
			{ "keyboard.6",         ActionData { ActionType::eKey, GLFW_KEY_6 } },
			{ "keyboard.7",         ActionData { ActionType::eKey, GLFW_KEY_7 } },
			{ "keyboard.8",         ActionData { ActionType::eKey, GLFW_KEY_8 } },
			{ "keyboard.9",         ActionData { ActionType::eKey, GLFW_KEY_9 } },
			{ "keyboard.0",         ActionData { ActionType::eKey, GLFW_KEY_0 } },
			{ "keyboard.space",     ActionData { ActionType::eKey, GLFW_KEY_SPACE       } },
			{ "keyboard.shift",     ActionData { ActionType::eKey, GLFW_KEY_LEFT_SHIFT  } },
			{ "keyboard.rshift",    ActionData { ActionType::eKey, GLFW_KEY_RIGHT_SHIFT } },
			{ "keyboard.ctrl",      ActionData { ActionType::eKey, GLFW_KEY_LEFT_CONTROL }},
			{ "keyboard.rctrl",     ActionData { ActionType::eKey, GLFW_KEY_RIGHT_CONTROL}},
			{ "keyboard.alt",       ActionData { ActionType::eKey, GLFW_KEY_LEFT_ALT    } },
			{ "keyboard.ralt",      ActionData { ActionType::eKey, GLFW_KEY_RIGHT_ALT   } },
			{ "keyboard.f1",        ActionData { ActionType::eKey, GLFW_KEY_F1     } },
			{ "keyboard.f2",        ActionData { ActionType::eKey, GLFW_KEY_F2     } },
			{ "keyboard.f3",        ActionData { ActionType::eKey, GLFW_KEY_F3     } },
			{ "keyboard.f4",        ActionData { ActionType::eKey, GLFW_KEY_F4     } },
			{ "keyboard.f5",        ActionData { ActionType::eKey, GLFW_KEY_F5     } },
			{ "keyboard.f6",        ActionData { ActionType::eKey, GLFW_KEY_F6     } },
			{ "keyboard.f7",        ActionData { ActionType::eKey, GLFW_KEY_F7     } },
			{ "keyboard.f8",        ActionData { ActionType::eKey, GLFW_KEY_F8     } },
			{ "keyboard.f9",        ActionData { ActionType::eKey, GLFW_KEY_F9     } },
			{ "keyboard.f10",       ActionData { ActionType::eKey, GLFW_KEY_F10    } },
			{ "keyboard.f11",       ActionData { ActionType::eKey, GLFW_KEY_F11    } },
			{ "keyboard.f12",       ActionData { ActionType::eKey, GLFW_KEY_F12    } },
			{ "keyboard.esc",       ActionData { ActionType::eKey, GLFW_KEY_ESCAPE } },
			{ "gamepad.a",          ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_A } },
			{ "gamepad.b",          ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_B } },
			{ "gamepad.x",          ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_X } },
			{ "gamepad.y",          ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_Y } },
			{ "gamepad.back",       ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_BACK  } },
			{ "gamepad.start",      ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_START } },
			{ "gamepad.lb",         ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_LEFT_BUMPER  } },
			{ "gamepad.rb",         ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER } },
			{ "gamepad.ls",         ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_LEFT_THUMB   } },
			{ "gamepad.rs",         ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_RIGHT_THUMB  } },
			{ "gamepad.lt",         ActionData { ActionType::eGamepad, GAMEPAD_LEFT_TRIGGER_BUTTON      } },
			{ "gamepad.rt",         ActionData { ActionType::eGamepad, GAMEPAD_RIGHT_TRIGGER_BUTTON     } },
			{ "gamepad.dpad_up",    ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_DPAD_UP      } },
			{ "gamepad.dpad_down",  ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_DPAD_DOWN    } },
			{ "gamepad.dpad_left",  ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_DPAD_LEFT    } },
			{ "gamepad.dpad_right", ActionData { ActionType::eGamepad, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT   } },
			{ "mouse.left_button",  ActionData { ActionType::eMouse, GLFW_MOUSE_BUTTON_LEFT   } },
			{ "mouse.middle_button",ActionData { ActionType::eMouse, GLFW_MOUSE_BUTTON_MIDDLE } },
			{ "mouse.right_button", ActionData { ActionType::eMouse, GLFW_MOUSE_BUTTON_RIGHT  } },
		};
	}
}

namespace lunar::Input
{
	InputHandler* GLOBAL_HANDLER = nullptr;

	InputHandler& GetGlobalHandler()
	{
		DEBUG_ASSERT(GLOBAL_HANDLER != nullptr, "Global handler was not set");
		return *GLOBAL_HANDLER;
	}

	void SetGlobalHandler(InputHandler* handler)
	{
		GLOBAL_HANDLER = handler;
		return;
	}

	void SetGlobalHandler(InputHandler& handler)
	{
		GLOBAL_HANDLER = &handler;
		return;
	}

	bool GetAction(const std::string_view& name)
	{
		return GetGlobalHandler().getAction(name);
	}

	bool GetActionUp(const std::string_view& name)
	{
		return GetGlobalHandler().getActionUp(name);
	}

	bool GetActionDown(const std::string_view& name)
	{
		return GetGlobalHandler().getActionDown(name);
	}

	glm::vec2 GetAxis()
	{
		return GetGlobalHandler().getAxis();
	}

	glm::vec2 GetRotation()
	{
		return GetGlobalHandler().getRotation();
	}

	float GetScroll()
	{
		return GetGlobalHandler().getScroll();
	}

	glm::vec2 GetTriggers()
	{
		return GetGlobalHandler().getTriggers();
	}

}

namespace lunar
{
	void InputHandler::registerAction(const std::string_view& name, const std::initializer_list<InputCombo>& combos)
	{
		DEBUG_ASSERT(combos.size() > 0 && combos.size() < 4, "Invalid combo count");
		DEBUG_ASSERT(!actions.contains(name), "Action already registered");

		auto input_keys = InputKeys{ };
		for (size_t i = 0; i < combos.size(); i++)
		{
			const auto& keys = *(combos.begin() + i);
			auto& values = input_keys.combos[i];

			DEBUG_ASSERT(keys.size() > 0 && keys.size() < 4, "Invalid key count");
			values.active = true;
			for (size_t i = 0; i < keys.size(); i++)
			{
				const auto& key = *(keys.begin() + i);
				DEBUG_ASSERT(imp::Actions.contains(key), "Inexistent key");
				values.key[i] = &imp::Actions.at(key);
			}
		}

		actions.insert(std::pair<std::string_view, InputKeys>(name, input_keys));
		DEBUG_LOG("Registered input action '{}'", name);
	}
}
