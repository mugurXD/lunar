#include <glfw/glfw3.h>

#include <lunar/core/platform.hpp>
#include <lunar/debug.hpp>


namespace lunar
{
	void Platform::initializeGlfwContext()
	{
		glfwInit();

		int major, minor, patch;
		glfwGetVersion(&major, &minor, &patch);
		DEBUG_LOG("GLFW initialized (version: {}.{}.{})", major, minor, patch);

		//glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		//int version = gladLoadGL(glfwGetProcAddress);
		//DEBUG_LOG("OpenGL context initialized (version: {}.{})", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
		//glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

	}

	void Platform::destroyGlfwContext()
	{
		glfwTerminate();
	}
}
