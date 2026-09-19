#include <lunar/debug/frame_stats.hpp>

#include <imgui.h>

#include <algorithm>
#include <format>
#include <numeric>
#include <span>
#include <string>

namespace lunar::Debug
{
	namespace
	{
		constexpr float MILLISECONDS_PER_SECOND = 1000.f;
		constexpr float GRAPH_HEIGHT            = 60.f;
		constexpr float GRAPH_HEADROOM          = 1.2f;
		constexpr char  WINDOW_TITLE[]          = "Frame stats";

		void Text(const std::string& text)
		{
			ImGui::TextUnformatted(text.c_str());
		}
	}

	void FrameStatsWindow::record(float frame_seconds)
	{
		history[next] = frame_seconds * MILLISECONDS_PER_SECOND;
		next          = (next + 1) % HISTORY_SIZE;
		count         = std::min(count + 1, HISTORY_SIZE);
	}

	void FrameStatsWindow::draw()
	{
		if (count == 0)
			return;

		const std::span<const float> recorded          = std::span(history.data(), count);
		const float                  average           = std::accumulate(recorded.begin(), recorded.end(), 0.f) / static_cast<float>(count);
		const auto                   [lowest, highest] = std::ranges::minmax(recorded);

		if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			Text(std::format("{:.0f} FPS", MILLISECONDS_PER_SECOND / average));
			Text(std::format("{:.2f} ms (min {:.2f}, max {:.2f})", average, lowest, highest));
			ImGui::PlotLines("##frame_times",
			                 recorded.data(),
			                 static_cast<int>(count),
			                 static_cast<int>(count < HISTORY_SIZE ? 0 : next),
			                 nullptr,
			                 0.f,
			                 highest * GRAPH_HEADROOM,
			                 ImVec2(static_cast<float>(HISTORY_SIZE), GRAPH_HEIGHT));
		}

		ImGui::End();
	}
}
