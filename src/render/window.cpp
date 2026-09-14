#ifdef LUNAR_VULKAN
//#	define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#	define GLFW_INCLUDE_VULKAN
#	include <lunar/render/internal/render_vk.hpp>
#	include <vulkan/vulkan.hpp>
#endif

#ifdef LUNAR_OPENGL
#	include <glad/gl.h>
#endif	

#include <lunar/file/config_file.hpp>
#include <lunar/render/window.hpp>
#include <lunar/render/context.hpp>
#include <lunar/debug.hpp>

#include <atomic>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace lunar::Render
{
	void GLFW_FramebufferSizeCb(GLFWwindow*, int, int);
	void GLFW_KeyCallback(GLFWwindow*, int, int, int, int);
	void GLFW_MouseBtnCallback(GLFWwindow*, int, int, int);
	void GLFW_CursorPosCb(GLFWwindow*, double, double);
	void GLFW_CursorEnterCb(GLFWwindow*, int);

	Window_T::Window_T
	(
		RenderContext_T*        context,
		int                     width,
		int                     height,
		bool                    fullscreen,
		const std::string_view& title,
		int                     msaa,
		bool                    vsync,
		Backend				    backend
	) noexcept : width(width),
		height(height),
		fullscreen(fullscreen),
		title(title),
		msaa(msaa),
		vsync(true),
		context(context)
	{
		glfwWindowHint(GLFW_SAMPLES, msaa);

		switch (backend)
		{
		case Backend::eVulkan:
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			break;
		case Backend::eOpenGL:
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			break;
		}
		
		this->handle = glfwCreateWindow(
			this->width, 
			this->height,
			this->title.c_str(),
			(fullscreen)
				? glfwGetPrimaryMonitor() // TODO: monitor selection
				: nullptr,
			imp::GetGlobalRenderContext()
				.glfw.headless
		);

		if (glfwRawMouseMotionSupported())
		{
			glfwSetInputMode(handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			DEBUG_LOG("Raw mouse motion support found.");
		}

		if (vsync)
		{
			glfwSwapInterval(2);
		}

		glfwMakeContextCurrent(handle);
		glfwSetWindowUserPointer(handle, this);

		glfwSetFramebufferSizeCallback(handle, GLFW_FramebufferSizeCb);
		glfwSetKeyCallback(handle,             GLFW_KeyCallback);
		glfwSetMouseButtonCallback(handle,     GLFW_MouseBtnCallback);
		glfwSetCursorPosCallback(handle,       GLFW_CursorPosCb);
		glfwSetCursorEnterCallback(handle,     GLFW_CursorEnterCb);

		IMGUI_CHECKVERSION();
		imguiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(imguiContext);

		switch (backend)
		{
		case Backend::eOpenGL:
			ImGui_ImplGlfw_InitForOpenGL(handle, true);
			ImGui_ImplOpenGL3_Init();
			initializeBackendData();
			break;
		default:
			break;
		}

		DEBUG_LOG("Window initialized.");
	}

	Window_T::~Window_T() noexcept
	{
		if (handle != nullptr)
		{
			clearBackendData();
			glfwDestroyWindow(handle);
			DEBUG_LOG("Window destroyed.");
		}
	}

	void Window_T::close()
	{
		glfwSetWindowShouldClose(handle, GLFW_TRUE);
	}

	void Window_T::pollEvents()
	{
		glfwPollEvents();
	}

	int Window_T::getRenderWidth() const
	{
		/* 
			Both functions need improving but I am too lazy right now 
			to implement the right solution for fullscreen toggling
		*/

		if (fullscreen)
		{
			GLFWmonitor*       monitor  = glfwGetWindowMonitor(handle);
			const GLFWvidmode* vid_mode = glfwGetVideoMode(monitor);
			
			return vid_mode->width;
		}

		return width;
	}

	int Window_T::getRenderHeight() const
	{
		if (fullscreen)
		{
			GLFWmonitor*       monitor  = glfwGetWindowMonitor(handle);
			const GLFWvidmode* vid_mode = glfwGetVideoMode(monitor);

			return vid_mode->height;
		}

		return height;
	}

	GLFWwindow* Window_T::glfwGetHandle()
	{
		return handle;
	}

	ImGuiContext* Window_T::imguiGetHandle()
	{
		return imguiContext;
	}

	bool Window_T::isActive() const
	{
		return handle != nullptr && glfwWindowShouldClose(handle) == GLFW_FALSE;
	}

	bool Window_T::isMinimized() const
	{
		return width == 0 || height == 0;
	}

	bool Window_T::isFullscreen() const
	{
		return fullscreen;
	}

	void Window_T::toggleFullscreen()
	{
		GLFWmonitor*       monitor  = glfwGetPrimaryMonitor();
		const GLFWvidmode* vid_mode = glfwGetVideoMode(monitor);

		fullscreen = !fullscreen;

		switch (fullscreen)
		{
		case true:  glfwSetWindowMonitor(handle, monitor, 0, 0, vid_mode->width, vid_mode->height, GLFW_DONT_CARE); break;
		case false: glfwSetWindowMonitor(handle, NULL, 5, 5, width, height, GLFW_DONT_CARE); break;
		}
	}

	void Window_T::setTitle(const std::string_view& title)
	{
		this->title = title;
		glfwSetWindowTitle(handle, this->title.c_str());
	}

	/*
		Input handling
	*/

	bool Window_T::isCursorLocked() const
	{
		return mouseLocked;
	}

	void Window_T::setCursorLocked(bool value)
	{
		glfwSetInputMode(handle, GLFW_CURSOR, value ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		mouseLocked = value;
	}

	void Window_T::toggleCursorLocked()
	{
		switch (mouseLocked)
		{
		case true:  setCursorLocked(false); return;
		case false: setCursorLocked(true);  return;
		}
	}

	inline void SetAxisValue(GLFWwindow* handle, int lower, int upper, float& value)
	{
		int first  = glfwGetKey(handle, lower);
		int second = glfwGetKey(handle, upper);

		value = 0.f;
		if (first == GLFW_PRESS)
			value += -1.f;
		if (second == GLFW_PRESS)
			value += 1.f;
	}

	void Window_T::update()
	{
		rotation = { 0, 0 };

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

		glfwSwapBuffers(handle);
	}

	inline int GetKeyId(::lunar::imp::ActionData* key)
	{
		switch (key->type)
		{
		case ::lunar::imp::ActionType::eKey:     return key->value;
		case ::lunar::imp::ActionType::eMouse:   return key->value | (1 << 31);
		case ::lunar::imp::ActionType::eGamepad: return key->value | (1 << 30);
		default: return key->value;
		}
	}

	bool Window_T::checkActionValue(const std::string_view& name, KeyState required) const
	{
		if (!actions.contains(name))
			return false;

		auto& options = actions.at(name);

		for (size_t i = 0; i < 4; i++)
		{
			auto& combo = options.combos[i];
			if (not combo.active)
				continue;

			bool combo_fulfilled = true;
			for (size_t j = 0; j < 4; j++)
			{
				auto& key = combo.key[j];
				if (key == nullptr)
					continue;

				int  id    = GetKeyId(key);
				auto value = keys.find(id);
				if (value == keys.end() || !(value->second & required))
					combo_fulfilled = false;
			}

			if (combo_fulfilled)
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

	imp::WindowBackendData& Window_T::getBackendData()
	{
		return this->imp;
	}

	/*
		Event handlers
	*/

	inline Window_T& GetWindowHandle(GLFWwindow* raw)
	{
		void* pointer = glfwGetWindowUserPointer(raw);
		Window_T* handle = static_cast<Window_T*>(pointer);
		return *handle;
	}

	void GLFW_MouseBtnCallback(GLFWwindow* handle, int button, int action, int mods)
	{
		static_assert(sizeof(int) == sizeof(int32_t));

		auto& window = GetWindowHandle(handle);

		window.keys[button | (int)(1 << 31)] = (action == GLFW_PRESS) 
			? KeyState::ePressed 
			: KeyState::eReleased;
	}

	void GLFW_KeyCallback(GLFWwindow* handle, int key, int scancode, int action, int mods)
	{
		auto& window = GetWindowHandle(handle);
		window.keys[key] = (action == GLFW_PRESS) 
			? KeyState::ePressed 
			: KeyState::eReleased;
	}

	void GLFW_FramebufferSizeCb(GLFWwindow* handle, int width, int height)
	{
		auto& window  = GetWindowHandle(handle);

		if (window.fullscreen)
			return;

		window.width  = width;
		window.height = height;
	}

	void GLFW_CursorPosCb(GLFWwindow* handle, double x, double y)
	{
		auto& window = GetWindowHandle(handle);
		if (not window.mouseInside)
			return;

		auto current = glm::vec2 { x, y };

		if (window.isCursorLocked())
		{
			auto delta = (current - window.lastMouse) * window.mouseSensitivity;
			window.rotation += glm::vec2{ delta.x, -delta.y };
		}

		window.lastMouse = current;
	}

	void GLFW_CursorEnterCb(GLFWwindow* handle, int entered)
	{
		auto& window = GetWindowHandle(handle);
		window.mouseInside = entered;
	}

	/*
		Global GLFW context
	*/

	namespace imp
	{
		GLFWGlobalContext::~GLFWGlobalContext() noexcept
		{
			glDeleteVertexArrays(1, &vao);

			glfwDestroyWindow(headless);
			glfwTerminate();
			DEBUG_LOG("Context destroyed.");
		}

		GLFWGlobalContext::GLFWGlobalContext() noexcept
		{
			glfwInit();

			int major, minor, patch;
			glfwGetVersion(&major, &minor, &patch);

			DEBUG_LOG("GLFW initialized (version: {}.{}.{})", major, minor, patch);

			glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // for Mac
			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
			glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

			this->headless = glfwCreateWindow(640, 480, "", nullptr, nullptr);
			glfwMakeContextCurrent(this->headless);

			int version = gladLoadGL(glfwGetProcAddress);
			DEBUG_LOG("OpenGL context initialized (version: {}.{})", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

			glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

			glGenVertexArrays(1, &vao);

			if (!GLAD_GL_ARB_bindless_texture)
			{
				DEBUG_LOG("Extension 'GL_ARB_bindless_texture' not supported on this device.");
				//abort();
			}
			else
			{
				DEBUG_LOG("Found support for all required OpenGL extensions.");
			}
		}
	}

	/*
		Window builder
	*/

	WindowBuilder& WindowBuilder::fullscreen(bool value)
	{
		this->isFullscreen = value;
		return *this;
	}

	WindowBuilder& WindowBuilder::size(int width, int height)
	{
		this->width  = width;
		this->height = height;
		return *this;
	}

	WindowBuilder& WindowBuilder::samples(int count)
	{
		this->msaa = count;
		return *this;
	}

	WindowBuilder& WindowBuilder::title(const std::string_view& title)
	{
		this->windowTitle = title;
		return *this;
	}

	WindowBuilder& WindowBuilder::renderBackend(Backend backend)
	{
		this->backend = backend;
		return *this;
	}

	Window_T WindowBuilder::build(RenderContext_T& context) const
	{
		return Window_T(
			&context,
			this->width,
			this->height,
			this->isFullscreen,
			this->windowTitle,
			this->msaa,
			this->enableVsync,
			this->backend
		);
	}
}
