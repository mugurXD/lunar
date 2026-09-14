#include <lunar/core/platform.hpp>

namespace lunar
{
	Platform::Platform() noexcept
	{
		initializeGlfwContext();
	}

	Platform::~Platform() noexcept
	{
		destroyGlfwContext();
	}
}
