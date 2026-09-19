#pragma once
#include <trok/world/biome.hpp>

#include <lunar/file/filesystem.hpp>

#include <memory>
#include <string>

namespace trok
{
	class BiomeTuningWindow
	{
	public:
		BiomeTuningWindow(std::shared_ptr<BiomeLibrary> biomes, Fs::Path save_path) noexcept;

		bool draw();

	private:
		void drawBiome(BiomeIndex index);
		void save();

		std::shared_ptr<BiomeLibrary> biomes;
		BiomeLibrary                  original;
		Fs::Path                      savePath;
		std::string                   status = {};
	};
}
