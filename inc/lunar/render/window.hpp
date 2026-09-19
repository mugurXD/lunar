#pragma once
#include <lunar/render/common.hpp>
#include <lunar/core/input.hpp>
#include <lunar/api.hpp>

#include <GLFW/glfw3.h>
#include <string>
#include <unordered_map>

namespace lunar::Render
{
	struct LUNAR_API Window_T : public InputHandler
	{
	public:
		Window_T
		(
			int                     width,
			int                     height,
			bool                    fullscreen,
			const std::string_view& title,
			int                     msaa,
			bool                    vsync,
			Backend				    backend
		) noexcept;
		Window_T()  noexcept = default;
		~Window_T() noexcept;

		Window_T(const Window_T&)             = delete; 
		Window_T(const Window_T&&)            = delete;
		Window_T& operator=(const Window_T&)  = delete;
		Window_T& operator=(const Window_T&&) = delete;


		void                    toggleFullscreen();
		void                    setFullscreen(bool value);
		void                    setTitle(const std::string_view& title);
		void                    toggleCursorLocked();
		void                    setCursorLocked(bool value);
		void                    update()                                    override;

		void                    close();
		bool                    isActive()                                  const;
		bool                    isMinimized()                               const;
		bool                    isFullscreen()                              const;
		bool                    isCursorLocked()                            const;
		int                     getRenderWidth()                            const;
		int                     getRenderHeight()                           const;
		bool                    getActionDown(const std::string_view& name) const override;
		bool                    getActionUp(const std::string_view& name)   const override;
		bool                    getAction(const std::string_view& name)     const override;
		glm::vec2               getAxis()                                   const override;
		glm::vec2               getRotation()                               const override;
		float                   getScroll()                                 const override;
		GLFWwindow*             glfwGetHandle();

		static void pollEvents();

	private:
		GLFWwindow*                       handle       = nullptr;
		int                               width        = -1;
		int                               height       = -1;
		int                               msaa         = -1;
		bool                              vsync        = false;
		bool                              fullscreen   = false;
		std::string                       title        = "lunar";
		std::unordered_map<int, KeyState> keys         = {};
		glm::vec2                         axis         = { 0, 0 };
		glm::vec2                         rotation     = { 0, 0 };
		float                             scroll       = 0.f;
		glm::vec2                         lastMouse    = { 0, 0 };
		bool                              mouseInside  = true;
		bool                              mouseLocked  = false;

		bool checkActionValue(const std::string_view& name, KeyState required) const;

		friend void GLFW_FramebufferSizeCb(GLFWwindow*, int, int);
		friend void GLFW_KeyCallback(GLFWwindow*, int, int, int, int);
		friend void GLFW_MouseBtnCallback(GLFWwindow*, int, int, int);
		friend void GLFW_CursorPosCb(GLFWwindow*, double, double);
		friend void GLFW_CursorEnterCb(GLFWwindow*, int);
		friend void GLFW_ScrollCb(GLFWwindow*, double, double);
	};

	struct LUNAR_API WindowBuilder
	{
	public:
		WindowBuilder()  noexcept = default;
		~WindowBuilder() noexcept = default;

		WindowBuilder& size(int width, int height);
		WindowBuilder& samples(int msaa);
		WindowBuilder& fullscreen(bool value);
		WindowBuilder& title(const std::string_view& title);
		WindowBuilder& renderBackend(Backend backend);
		Window_T       build() const;

	private:
		int              width        = -1;
		int              height       = -1;
		int              msaa         = 0;
		bool             isFullscreen = false;
		bool             enableVsync  = false;
		std::string_view windowTitle  = "<no title>";
		Backend          backend      = Backend::eDefault;
	};
}
