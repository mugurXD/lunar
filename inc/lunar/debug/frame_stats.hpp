#pragma once
#include <lunar/api.hpp>

#include <array>
#include <cstddef>

namespace lunar::Debug
{
	class LUNAR_API FrameStatsWindow
	{
	public:
		static constexpr size_t HISTORY_SIZE = 240;

		void record(float frame_seconds);
		void toggle();
		void draw();

	private:
		std::array<float, HISTORY_SIZE> history = {};
		size_t                          count   = 0;
		size_t                          next    = 0;
		bool                            visible = false;
	};
}
