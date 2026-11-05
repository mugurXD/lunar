#include <lunar/debug/ambient_occlusion_window.hpp>

#include <imgui.h>

namespace lunar::Debug
{
	namespace
	{
		constexpr char WINDOW_TITLE[]  = "Ambient occlusion";
		constexpr char SLIDER_FORMAT[] = "%.2f";

		struct FloatSetting
		{
			const char*               label   = nullptr;
			float AmbientOcclusion::* value   = nullptr;
			float                     minimum = 0.f;
			float                     maximum = 0.f;
		};

		constexpr FloatSetting SETTINGS[] =
		{
			{ "Radius",           &AmbientOcclusion::radius,          0.1f,  10.f },
			{ "Intensity",        &AmbientOcclusion::intensity,       0.f,   8.f },
			{ "Distance",         &AmbientOcclusion::distance,        10.f,  1000.f },
			{ "Resolution scale", &AmbientOcclusion::resolutionScale, 0.25f, 1.f }
		};
	}

	void DrawAmbientOcclusionWindow(AmbientOcclusion& occlusion)
	{
		if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			for (const FloatSetting& setting : SETTINGS)
				ImGui::SliderFloat(setting.label, &(occlusion.*setting.value), setting.minimum, setting.maximum, SLIDER_FORMAT);

			if (ImGui::Button("Reset"))
				occlusion = {};
		}

		ImGui::End();
	}
}
