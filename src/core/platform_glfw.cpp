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
	}

	void Platform::destroyGlfwContext()
	{
		glfwTerminate();
	}
}
