#include <lunar/core/time.hpp>
#include <lunar/debug.hpp>
#include <GLFW/glfw3.h>
#include <vector>
#include <mutex>

namespace lunar::Time
{
	void TimeContext_T::update()
	{
		lastTime    = currentTime.load();
		currentTime = glfwGetTime();
		deltaTime   = currentTime - lastTime;

		frames      = frames + 1;

		if (currentTime - timer >= 1.f)
		{
			fps    = frames / (currentTime - timer);
			timer  = currentTime.load();
			frames = 0;
		}
	}
}
