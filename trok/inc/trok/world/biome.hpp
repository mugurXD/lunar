#pragma once
#include <trok/world/climate.hpp>

#include <lunar/file/json_file.hpp>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trok
{
	using BiomeIndex = uint16_t;

	struct BiomeTerrain
	{
		float   baseHeight = 0.f;
		float   amplitude  = 30.f;
		float   frequency  = 0.002f;
		int32_t octaves    = 4;
		bool    ridged     = false;

		bool operator==(const BiomeTerrain&) const = default;
	};

	struct BiomeColors
	{
		glm::vec3 lowColor       = { 0.10f, 0.22f, 0.06f };
		glm::vec3 highColor      = { 0.22f, 0.36f, 0.10f };
		glm::vec3 rockColor      = { 0.32f, 0.30f, 0.28f };
		float     rockSlopeStart = 0.25f;
		float     rockSlopeEnd   = 0.45f;

		bool operator==(const BiomeColors&) const = default;
	};

	struct ClimateRange
	{
		float min = -1.f;
		float max =  1.f;

		bool operator==(const ClimateRange&) const = default;

		float distanceTo(float value) const;
	};

	struct BiomeClimate
	{
		ClimateRange temperature     = {};
		ClimateRange moisture        = {};
		ClimateRange continentalness = {};

		bool operator==(const BiomeClimate&) const = default;
	};

	struct Biome
	{
		std::string  name    = {};
		BiomeClimate climate = {};
		BiomeTerrain terrain = {};
		BiomeColors  colors  = {};

		bool operator==(const Biome&) const = default;

		float distanceTo(const Climate& other) const;

		static nlohmann::json       Serialize(const Biome& biome);
		static std::optional<Biome> Deserialize(const nlohmann::json& json);
	};

	class BiomeLibrary
	{
	public:
		const std::vector<Biome>& getBiomes()                                          const;
		std::vector<std::string>  getNames()                                           const;
		const Biome&              get(BiomeIndex index)                                const;
		BiomeIndex                getDefault()                                         const;
		std::optional<BiomeIndex> indexOf(std::string_view name)                       const;
		BiomeIndex                select(const Climate& climate, uint64_t tie_breaker) const;

		bool operator==(const BiomeLibrary&) const = default;

		static std::optional<BiomeLibrary> Create(std::vector<Biome> biomes, std::string_view default_biome);
		static nlohmann::json              Serialize(const BiomeLibrary& library);
		static std::optional<BiomeLibrary> Deserialize(const nlohmann::json& json);

	private:
		std::vector<Biome> biomes       = { Biome {} };
		BiomeIndex         defaultBiome = 0;
	};
}
