#pragma once
#include <lunar/render/common.hpp>
#include <lunar/render/render_target.hpp>
#include <lunar/file/config_file.hpp>
#include <lunar/core/input.hpp>
#include <lunar/api.hpp>

#include <GLFW/glfw3.h>
#include <unordered_map>
#include <imgui.h>

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
			RenderContext_T*        context,
			int                     width,
			int                     height,
			bool                    fullscreen,
			const std::string_view& title,
			int                     msaa,
			bool                    vsync
		) noexcept;
		Window_T()  noexcept = default;
		~Window_T() noexcept;

		Window_T(const Window_T&)             = delete; 
		Window_T(const Window_T&&)            = delete;
		Window_T& operator=(const Window_T&)  = delete;
		Window_T& operator=(const Window_T&&) = delete;


		void                    toggleFullscreen();
		void                    setFullscreen(bool value);
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
		RenderContext_T*                  context      = nullptr;
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
		Window_T       build(RenderContext_T& context) const;

	private:
		int              width        = -1;
		int              height       = -1;
		int              msaa         = 0;
		bool             isFullscreen = false;
		bool             enableVsync  = false;
		std::string_view windowTitle  = "<no title>";
	};

	namespace imp
	{
		struct LUNAR_API GLFWGlobalContext
		{
		public:
			GLFWGlobalContext()  noexcept;
			~GLFWGlobalContext() noexcept;

			GLFWwindow* headless = nullptr;
			GLuint      vao      = 0;
		};
	}
}

//namespace lunar::Render
//{
//#	ifdef LUNAR_VULKAN
//	constexpr size_t FRAME_OVERLAP = 2;
//
//	struct VulkanFrameData
//	{
//		struct
//		{
//			vk::Image image;
//			vk::ImageView view;
//			vk::Semaphore imageAvailable;
//		} swapchain;
//
//		struct
//		{
//			VulkanImage image;
//			VulkanImage depthImage;
//			vk::Extent2D extent;
//			vk::Semaphore renderFinished;
//		} internal;
//
//		struct
//		{
//			vk::DescriptorSetLayout descriptorLayout;
//			vk::DescriptorSet descriptorSet;
//			VulkanBuffer buffer;
//		} uniformBuffer;
//
//		VulkanCommandBuffer commandBuffer;
//	};
//#	endif
//
//
//	class LUNAR_API Window : public RenderTarget, public InputHandler
//	{
//	public:
//		Window(
//			int width,
//			int height,
//			bool fullscreen,
//			const char* title,
//			RenderContext context
//		);
//
//		~Window();
//
//		void      init(int width, int height, bool fullscreen, const char* title, RenderContext context);
//		void        destroy();
//		void        close();
//		void        update() override;
//		bool        shouldClose() const;
//		bool        isMinimized() const;
//		bool        exists() const;
//		void        lockCursor();
//		void        unlockCursor();
//		void        toggleCursor();
//		bool        isCursorLocked() const;
//
//		int         getRenderWidth() const override;
//		int         getRenderHeight() const override;
//
//		bool        getActionDown(const std::string_view& name) const override;
//		bool        getActionUp(const std::string_view& name) const override;
//		bool        getAction(const std::string_view& name) const override;
//		glm::vec2   getAxis()     const override;
//		glm::vec2   getRotation() const override;
//
//
//		static void pollEvents();
//
//#		ifdef LUNAR_VULKAN
//		vk::SurfaceKHR& getVkSurface();
//		vk::SwapchainKHR& getVkSwapchain();
//		VulkanFrameData& getVkFrameData(size_t idx);
//		size_t getVkSwapImageCount();
//		const vk::Extent2D& getVkSwapExtent() const;
//
//		vk::Extent2D& getVkSwapExtent();
//		vk::Image& getVkSwapImage(size_t idx);
//		vk::Semaphore& getVkImageAvailable(size_t idx);
//		vk::Semaphore& getVkImagePresentable(size_t idx);
//		VulkanCommandBuffer& getVkCommandBuffer(size_t idx);
//
//
//		size_t getVkCurrentFrame() const;
//		void endVkFrame();
//#		endif
//	protected:
//		GLFWwindow*                       handle      = nullptr;
//		RenderContext                     renderCtx   = nullptr;
//		bool                              initialized = false;
//		std::unordered_map<int, KeyState> keys        = {};
//		glm::vec2                         axis        = { 0, 0 };
//		glm::vec2                         rotation    = { 0, 0 };
//		glm::vec2                         lastMouse   = { 0, 0 };
//		bool                              mouseInside = false;
//		bool                              mouseLocked = false;
//
//		bool checkActionValue(const std::string_view& name, KeyState required) const;
//
//		friend void Glfw_FramebufferSizeCb(GLFWwindow*, int, int);
//		friend void Glfw_KeyCallback(GLFWwindow*, int, int, int, int);
//		friend void Glfw_MouseBtnCallback(GLFWwindow*, int, int, int);
//		friend void Glfw_CursorPosCb(GLFWwindow*, double, double);
//		friend void Glfw_CursorEnterCb(GLFWwindow*, int);
//
//#		ifdef LUNAR_OPENGL
//		void _glInitialize();
//#		endif
//
//#		ifdef LUNAR_VULKAN
//		vk::SurfaceKHR _vkSurface;
//		vk::SurfaceFormatKHR _vkSurfaceFmt;
//		vk::PresentModeKHR _vkPresentMode;
//		
//		vk::SwapchainKHR _vkSwapchain;
//		vk::Extent2D _vkSwapExtent;
//		size_t _vkSwapImgCount;
//
//		VulkanFrameData _vkFrameData[FRAME_OVERLAP];
//		VulkanCommandPool _vkCommandPool;
//		VulkanGrowableDescriptorAllocator _vkDescriptorAlloc;
//		size_t _vkCurrentFrame;
//
//		VulkanContext& _getVkContext();
//
//		void _vkInitialize();
//		void _vkInitSwap();
//		void _vkDestroy();
//		void _vkDestroySwap();
//		void _vkUpdateSwapExtent();
//		void _vkHandleResize(int width, int height);
//
//		friend class VulkanContext;
//#		endif
//	};
//
//	struct LUNAR_API WindowBuilder
//	{
//	public:
//		WindowBuilder& setWidth(int width);
//		WindowBuilder& setHeight(int height);
//		WindowBuilder& setSize(int width, int height);
//		WindowBuilder& setFullscreen(bool value = true);
//		WindowBuilder& setTitle(const std::string_view& title);
//		WindowBuilder& setRenderContext(RenderContext context);
//		WindowBuilder& setDefaultRenderContext();
//		WindowBuilder& loadFromConfigFile(const Fs::Path& path);
//		Window create();
//
//	private:
//		int w = 800, h = 600;
//		bool fs = false;
//		std::string_view title = "lunar";
//		RenderContext renderContext = nullptr;
//	};
//}
