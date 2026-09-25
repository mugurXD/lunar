#include <trok/world/biome_tuning.hpp>

#include <lunar/file/json_file.hpp>

#include <imgui.h>

#include <format>
#include <utility>

namespace trok
{
	namespace
	{
		constexpr char  WINDOW_TITLE[]       = "Biomes";
		constexpr char  SLIDER_FORMAT[]      = "%.4f";
		constexpr int   READABLE_INDENTATION = 4;
		constexpr int   MIN_OCTAVES          = 1;
		constexpr int   MAX_OCTAVES          = 8;
		constexpr float MIN_SLOPE            = 0.f;
		constexpr float MAX_SLOPE            = 1.f;
		constexpr float MIN_BLEND_WIDTH      = 0.01f;
		constexpr float MAX_BLEND_WIDTH      = 0.5f;

		struct TerrainSetting
		{
			const char*           label       = nullptr;
			float BiomeTerrain::* value       = nullptr;
			float                 minimum     = 0.f;
			float                 maximum     = 0.f;
			bool                  logarithmic = false;
		};

		struct ColorSetting
		{
			const char*              label = nullptr;
			glm::vec3 BiomeColors::* value = nullptr;
		};

		struct SlopeSetting
		{
			const char*          label = nullptr;
			float BiomeColors::* value = nullptr;
		};

		constexpr TerrainSetting TERRAIN_SETTINGS[] =
		{
			{ "Height offset", &BiomeTerrain::heightOffset, -50.f,   200.f },
			{ "Amplitude",     &BiomeTerrain::amplitude,    0.f,     200.f },
			{ "Frequency",     &BiomeTerrain::frequency,    0.0001f, 0.01f, true }
		};

		constexpr ColorSetting COLOR_SETTINGS[] =
		{
			{ "Low colour",  &BiomeColors::lowColor },
			{ "High colour", &BiomeColors::highColor },
			{ "Rock colour", &BiomeColors::rockColor }
		};

		constexpr SlopeSetting SLOPE_SETTINGS[] =
		{
			{ "Rock slope start", &BiomeColors::rockSlopeStart },
			{ "Rock slope end",   &BiomeColors::rockSlopeEnd }
		};

		void Text(const std::string& text)
		{
			ImGui::TextUnformatted(text.c_str());
		}
	}

	BiomeTuningWindow::BiomeTuningWindow(std::shared_ptr<BiomeLibrary> biomes, Fs::Path save_path) noexcept
		: biomes(std::move(biomes)),
		original(*this->biomes),
		savePath(std::move(save_path))
	{
	}

	bool BiomeTuningWindow::draw()
	{
		bool regenerate = false;

		if (ImGui::Begin(WINDOW_TITLE, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			float blend_width = biomes->getBlendWidth();
			if (ImGui::SliderFloat("Blend width", &blend_width, MIN_BLEND_WIDTH, MAX_BLEND_WIDTH, SLIDER_FORMAT))
				biomes->setBlendWidth(blend_width);

			for (size_t index = 0; index < biomes->getBiomes().size(); index++)
				drawBiome(static_cast<BiomeIndex>(index));

			if (ImGui::Button("Reset all"))
				*biomes = original;

			ImGui::SameLine();
			if (ImGui::Button("Save"))
				save();

			ImGui::SameLine();
			regenerate = ImGui::Button("Regenerate chunks");

			if (!status.empty())
				Text(status);
		}

		ImGui::End();
		return regenerate;
	}

	void BiomeTuningWindow::drawBiome(BiomeIndex index)
	{
		Biome& biome = biomes->editBiome(index);

		ImGui::PushID(static_cast<int>(index));
		if (ImGui::CollapsingHeader(biome.name.c_str()))
		{
			for (const TerrainSetting& setting : TERRAIN_SETTINGS)
				ImGui::SliderFloat(setting.label,
				                   &(biome.terrain.*setting.value),
				                   setting.minimum,
				                   setting.maximum,
				                   SLIDER_FORMAT,
				                   setting.logarithmic ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None);

			ImGui::SliderInt("Octaves", &biome.terrain.octaves, MIN_OCTAVES, MAX_OCTAVES);
			ImGui::Checkbox("Ridged", &biome.terrain.ridged);

			for (const ColorSetting& setting : COLOR_SETTINGS)
				ImGui::ColorEdit3(setting.label, &(biome.colors.*setting.value).x);

			for (const SlopeSetting& setting : SLOPE_SETTINGS)
				ImGui::SliderFloat(setting.label, &(biome.colors.*setting.value), MIN_SLOPE, MAX_SLOPE);

			if (ImGui::Button("Reset biome"))
				biome = original.get(index);
		}

		ImGui::PopID();
	}

	void BiomeTuningWindow::save()
	{
		Fs::JsonFile file;
		file.content     = BiomeLibrary::Serialize(*biomes);
		file.indentation = READABLE_INDENTATION;

		status = file.toFile(savePath) ? std::format("Saved to {}", savePath.string()) : std::format("Could not save to {}", savePath.string());
	}
}
