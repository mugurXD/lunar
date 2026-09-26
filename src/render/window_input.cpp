#include <lunar/render/window.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <optional>

namespace lunar::Render
{
	namespace
	{
		constexpr float CURSOR_UV_CENTER       = 0.5f;
		constexpr float HALF                   = 0.5f;
		constexpr float TRIGGER_PRESS_POINT    = 0.5f;
		constexpr float MAX_LOOK_DELTA_SECONDS = 0.1f;
		constexpr int   MOUSE_KEY_FLAG         = 1 << 31;
		constexpr int   GAMEPAD_KEY_FLAG       = 1 << 30;

		void SetAxisValue(GLFWwindow* handle, int lower, int upper, float& value)
		{
			value = 0.f;
			if (glfwGetKey(handle, lower) == GLFW_PRESS)
				value -= 1.f;
			if (glfwGetKey(handle, upper) == GLFW_PRESS)
				value += 1.f;
		}

		int GetKeyId(const ::lunar::imp::ActionData* key)
		{
			switch (key->type)
			{
			case ::lunar::imp::ActionType::eMouse:   return static_cast<int>(key->value) | MOUSE_KEY_FLAG;
			case ::lunar::imp::ActionType::eGamepad: return static_cast<int>(key->value) | GAMEPAD_KEY_FLAG;
			default:                                 return static_cast<int>(key->value);
			}
		}

		std::optional<GLFWgamepadstate> FirstGamepad()
		{
			for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; joystick++)
			{
				GLFWgamepadstate state;
				if (glfwJoystickIsGamepad(joystick) == GLFW_TRUE && glfwGetGamepadState(joystick, &state) == GLFW_TRUE)
					return state;
			}

			return std::nullopt;
		}

		glm::vec2 ApplyDeadzone(const glm::vec2& stick, float deadzone)
		{
			const float length = glm::length(stick);
			if (length <= deadzone)
				return { 0.f, 0.f };

			return stick / length * (std::min(length, 1.f) - deadzone) / (1.f - deadzone);
		}

		float TriggerAmount(float raw)
		{
			return (raw + 1.f) * HALF;
		}

		Window_T& GetWindowHandle(GLFWwindow* raw)
		{
			return *static_cast<Window_T*>(glfwGetWindowUserPointer(raw));
		}

		KeyState NextState(bool pressed)
		{
			return pressed ? KeyState::ePressed : KeyState::eReleased;
		}
	}

	bool Window_T::isCursorLocked() const
	{
		return mouseLocked;
	}

	glm::vec2 Window_T::getCursorUv() const
	{
		int content_width  = 0;
		int content_height = 0;
		glfwGetWindowSize(handle, &content_width, &content_height);

		if (mouseLocked || content_width <= 0 || content_height <= 0)
			return glm::vec2(CURSOR_UV_CENTER);

		return lastMouse / glm::vec2(content_width, content_height);
	}

	void Window_T::setCursorLocked(bool value)
	{
		glfwSetInputMode(handle, GLFW_CURSOR, value ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		mouseLocked = value;
	}

	void Window_T::toggleCursorLocked()
	{
		setCursorLocked(!mouseLocked);
	}

	void Window_T::update()
	{
		const double now        = glfwGetTime();
		const float  delta_time = std::min(static_cast<float>(now - lastUpdate), MAX_LOOK_DELTA_SECONDS);
		lastUpdate = now;

		rotation = { 0, 0 };
		scroll   = 0.f;

		for (auto& [key, value] : keys)
		{
			switch (value)
			{
			case KeyState::ePressed:  value = KeyState::eHeld; break;
			case KeyState::eReleased: value = KeyState::eNone; break;
			default: break;
			}
		}

		SetAxisValue(handle, GLFW_KEY_A, GLFW_KEY_D, axis.x);
		SetAxisValue(handle, GLFW_KEY_S, GLFW_KEY_W, axis.y);
		updateGamepad(delta_time);
	}

	void Window_T::updateGamepad(float delta_time)
	{
		const std::optional<GLFWgamepadstate> state = FirstGamepad();
		const auto                            down  = [&](int button) { return state.has_value() && state->buttons[button] == GLFW_PRESS; };

		for (int button = 0; button <= GLFW_GAMEPAD_BUTTON_LAST; button++)
			setGamepadButton(button, down(button));

		triggers = state.has_value()
		         ? glm::vec2(TriggerAmount(state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]), TriggerAmount(state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]))
		         : glm::vec2(0.f);

		setGamepadButton(imp::GAMEPAD_LEFT_TRIGGER_BUTTON,  triggers.x >= TRIGGER_PRESS_POINT);
		setGamepadButton(imp::GAMEPAD_RIGHT_TRIGGER_BUTTON, triggers.y >= TRIGGER_PRESS_POINT);

		if (!state.has_value())
			return;

		const glm::vec2 move = ApplyDeadzone({ state->axes[GLFW_GAMEPAD_AXIS_LEFT_X],  -state->axes[GLFW_GAMEPAD_AXIS_LEFT_Y] },  deadzoneLeft);
		const glm::vec2 look = ApplyDeadzone({ state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X], -state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] }, deadzoneRight);

		axis      = glm::clamp(axis + move, glm::vec2(-1.f), glm::vec2(1.f));
		rotation += look * gamepadLookSpeed * delta_time;
	}

	void Window_T::setGamepadButton(int button, bool down)
	{
		KeyState&  state = keys[button | GAMEPAD_KEY_FLAG];
		const bool held  = state & (KeyState::ePressed | KeyState::eHeld);

		if (down != held)
			state = NextState(down);
	}

	bool Window_T::checkActionValue(const std::string_view& name, KeyState required) const
	{
		const auto found = actions.find(name);
		if (found == actions.end())
			return false;

		for (const InputComboValues& combo : found->second.combos)
		{
			if (!combo.active)
				continue;

			const bool fulfilled = std::ranges::all_of(combo.key, [&](const imp::ActionData* key) {
				if (key == nullptr)
					return true;

				const auto value = keys.find(GetKeyId(key));
				return value != keys.end() && (value->second & required);
			});

			if (fulfilled)
				return true;
		}

		return false;
	}

	bool Window_T::getAction(const std::string_view& name) const
	{
		return checkActionValue(name, KeyState::ePressed | KeyState::eHeld | KeyState::eReleased);
	}

	bool Window_T::getActionUp(const std::string_view& name) const
	{
		return checkActionValue(name, KeyState::eReleased);
	}

	bool Window_T::getActionDown(const std::string_view& name) const
	{
		return checkActionValue(name, KeyState::ePressed);
	}

	glm::vec2 Window_T::getAxis() const
	{
		return axis;
	}

	glm::vec2 Window_T::getRotation() const
	{
		return rotation;
	}

	float Window_T::getScroll() const
	{
		return scroll;
	}

	glm::vec2 Window_T::getTriggers() const
	{
		return triggers;
	}

	void GLFW_MouseBtnCallback(GLFWwindow* handle, int button, int action, int)
	{
		GetWindowHandle(handle).keys[button | MOUSE_KEY_FLAG] = NextState(action == GLFW_PRESS);
	}

	void GLFW_KeyCallback(GLFWwindow* handle, int key, int, int action, int)
	{
		if (action != GLFW_REPEAT)
			GetWindowHandle(handle).keys[key] = NextState(action == GLFW_PRESS);
	}

	void GLFW_CursorPosCb(GLFWwindow* handle, double x, double y)
	{
		Window_T& window = GetWindowHandle(handle);
		if (!window.mouseInside)
			return;

		const glm::vec2 current = { x, y };
		if (window.isCursorLocked())
		{
			const glm::vec2 delta = (current - window.lastMouse) * window.mouseSensitivity;
			window.rotation += glm::vec2 { delta.x, -delta.y };
		}

		window.lastMouse = current;
	}

	void GLFW_ScrollCb(GLFWwindow* handle, double, double y_offset)
	{
		Window_T& window = GetWindowHandle(handle);
		if (window.isCursorLocked())
			window.scroll += static_cast<float>(y_offset);
	}

	void GLFW_CursorEnterCb(GLFWwindow* handle, int entered)
	{
		GetWindowHandle(handle).mouseInside = entered;
	}
}
