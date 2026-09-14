//#include <lunar/render/context.hpp>
//#include <lunar/render/window.hpp>
//#include <lunar/debug/log.hpp>
//
//#include <GLFW/glfw3.h>
//#include <imgui.h>
//
//namespace lunar::Render
//{
//	void GLAPIENTRY DebugCallback
//	(
//		GLenum        source,
//		GLenum        type,
//		GLuint        id,
//		GLenum        severity,
//		GLsizei       length,
//		const GLchar* message,
//		const void* userParam
//	);
//
//	RenderContext_T::RenderContext_T() noexcept
//	{
//		/*
//			Calling the function assures the static variable inside of it (i.e.: the global
//			context) gets initialized.
//		*/
//		imp::GetGlobalRenderContext();
//
//		loadDefaultPrograms();
//		loadDefaultMeshes();
//
//		glGenFramebuffers(1, &frameBuffer);
//		glGenRenderbuffers(1, &renderBuffer);
//	}
//
//	RenderContext_T::~RenderContext_T() noexcept
//	{
//		cubemaps.clear();
//		meshes.clear();
//		programs.clear();
//		textures.clear();
//		buffers.clear();
//		//vertexArrays.clear();
//
//		glDeleteFramebuffers(1, &frameBuffer);
//		glDeleteRenderbuffers(1, &renderBuffer);
//
//		glfwDestroyWindow(headless);
//	}
//
//	void RenderContext_T::setViewportSize(int width, int height)
//	{
//		this->viewportWidth  = width;
//		this->viewportHeight = height;
//		glViewport(0, 0, width, height);
//	}
//
//	void Window_T::initializeBackendData()
//	{
//		int flags;
//		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
//		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
//		{
//			glEnable(GL_DEBUG_OUTPUT);
//			glDebugMessageCallback(DebugCallback, nullptr);
//			DEBUG_LOG("OpenGL debugging enabled.");
//		}
//
//		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
//		glEnable(GL_DEPTH_TEST);
//		glDepthFunc(GL_LEQUAL);
//		glDisable(GL_CULL_FACE);
//
//		glGenVertexArrays(1, &imp.globalVao);
//
//		imp.sceneDataUniform = context->createBuffer(
//			GpuBufferType::eUniform,
//			GpuBufferUsageFlagBits::eDynamic,
//			sizeof(imp::GpuSceneData),
//			nullptr
//		);
//
//		imp.meshDataUniform = context->createBuffer(
//			GpuBufferType::eUniform,
//			GpuBufferUsageFlagBits::eDynamic,
//			sizeof(imp::GpuMeshData),
//			nullptr
//		);
//	}
//
//	void Window_T::clearBackendData()
//	{
//		imp.meshDataUniform = nullptr;
//		glDeleteVertexArrays(1, &imp.globalVao);
//	}
//
//	void GLAPIENTRY DebugCallback
//	(
//		GLenum        source,
//		GLenum        type,
//		GLuint        id,
//		GLenum        severity,
//		GLsizei       length,
//		const GLchar* message,
//		const void* userParam
//	)
//	{
//		if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
//			DEBUG_ERROR("OpenGL error: {}", message);
//	}
//}
