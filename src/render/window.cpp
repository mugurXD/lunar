#include <lunar/render/window.hpp>
#include <lunar/debug.hpp>

#include <GLFW/glfw3.h>

namespace lunar::Render
{
	void GLFW_FramebufferSizeCb(GLFWwindow*, int, int);
	void GLFW_KeyCallback(GLFWwindow*, int, int, int, int);
	void GLFW_MouseBtnCallback(GLFWwindow*, int, int, int);
	void GLFW_CursorPosCb(GLFWwindow*, double, double);
	void GLFW_CursorEnterCb(GLFWwindow*, int);
	void GLFW_ScrollCb(GLFWwindow*, double, double);

	Window_T::Window_T
	(
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
		vsync(true)
	{
		switch (backend)
		{
		case Backend::eVulkan:
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			break;
		}
		
		this->handle = glfwCreateWindow(
			this->width, 
			this->height,
			this->title.c_str(),
			(fullscreen)
				? glfwGetPrimaryMonitor() // TODO: monitor selection
				: nullptr,
			nullptr
		);

		if (glfwRawMouseMotionSupported())
		{
			glfwSetInputMode(handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			DEBUG_LOG("Raw mouse motion support found.");
		}

		glfwSetWindowUserPointer(handle, this);

		glfwSetFramebufferSizeCallback(handle, GLFW_FramebufferSizeCb);
		glfwSetKeyCallback(handle,             GLFW_KeyCallback);
		glfwSetMouseButtonCallback(handle,     GLFW_MouseBtnCallback);
		glfwSetCursorPosCallback(handle,       GLFW_CursorPosCb);
		glfwSetCursorEnterCallback(handle,     GLFW_CursorEnterCb);
		glfwSetScrollCallback(handle,          GLFW_ScrollCb);

		lastUpdate = glfwGetTime();

		DEBUG_LOG("Window initialized.");
	}

	Window_T::~Window_T() noexcept
	{
		if (handle != nullptr)
		{
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

	void GLFW_FramebufferSizeCb(GLFWwindow* handle, int width, int height)
	{
		Window_T& window = *static_cast<Window_T*>(glfwGetWindowUserPointer(handle));

		if (window.fullscreen)
			return;

		window.width  = width;
		window.height = height;
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

	Window_T WindowBuilder::build() const
	{
		return Window_T(
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
