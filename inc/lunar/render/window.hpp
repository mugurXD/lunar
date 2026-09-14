#pragma once
#include <lunar/render/common.hpp>
#include <lunar/render/render_target.hpp>
#include <lunar/file/config_file.hpp>
#include <lunar/core/input.hpp>
#include <lunar/api.hpp>

#include <GLFW/glfw3.h>
#include <unordered_map>
#include <imgui.h>

#include <lunar/render/common.hpp>

#ifdef LUNAR_VULKAN
#	include <lunar/render/internal/render_vk.hpp>
#	include <vulkan/vulkan.hpp>
#endif

#ifdef LUNAR_OPENGL
#	include <lunar/render/imp/gl/window.hpp>
#endif

namespace lunar::Render
{
	struct LUNAR_API Window_T : public RenderTarget, public InputHandler
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
		int                     getRenderWidth()                            const override;
		int                     getRenderHeight()                           const override;
		bool                    getActionDown(const std::string_view& name) const override;
		bool                    getActionUp(const std::string_view& name)   const override;
		bool                    getAction(const std::string_view& name)     const override;
		glm::vec2               getAxis()                                   const override;
		glm::vec2               getRotation()                               const override;
		GLFWwindow*             glfwGetHandle();
		ImGuiContext*           imguiGetHandle();
		imp::WindowBackendData& getBackendData();

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
		glm::vec2                         lastMouse    = { 0, 0 };
		bool                              mouseInside  = true;
		bool                              mouseLocked  = false;
		ImGuiContext*                     imguiContext = nullptr;
		imp::WindowBackendData            imp          = {};

		bool checkActionValue(const std::string_view& name, KeyState required) const;
		void initializeBackendData();
		void clearBackendData();

		friend void GLFW_FramebufferSizeCb(GLFWwindow*, int, int);
		friend void GLFW_KeyCallback(GLFWwindow*, int, int, int, int);
		friend void GLFW_MouseBtnCallback(GLFWwindow*, int, int, int);
		friend void GLFW_CursorPosCb(GLFWwindow*, double, double);
		friend void GLFW_CursorEnterCb(GLFWwindow*, int);
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
