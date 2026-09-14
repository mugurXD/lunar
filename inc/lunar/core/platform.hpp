#pragma once
#include <lunar/api.hpp>

namespace lunar
{
	class LUNAR_API Platform
	{
	public:
		Platform()  noexcept;
		~Platform() noexcept;

	private:
		void initializeGlfwContext();
		void destroyGlfwContext();
	};
}
