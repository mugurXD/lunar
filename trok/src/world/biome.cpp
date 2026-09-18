#include <trok/world/biome.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string_view>

namespace trok
{
	namespace
	{
		constexpr uint32_t BIOME_FORMAT_VERSION = 2;

		constexpr std::string_view TEMPERATURE_KEY     = "temperature";
		constexpr std::string_view MOISTURE_KEY        = "moisture";
		constexpr std::string_view CONTINENTALNESS_KEY = "continentalness";
		constexpr std::string_view EROSION_KEY         = "erosion";

		nlohmann::json SerializeRange(const ClimateRange& range)
		{
			return nlohmann::json { range.min, range.max };
		}

		ClimateRange DeserializeRange(const nlohmann::json& json, std::string_view key)
		{
			if (!json.contains(key))
				return ClimateRange {};

			const nlohmann::json& range = json.at(key);
			return ClimateRange { .min = range.at(0).get<float>(), .max = range.at(1).get<float>() };
		}

		nlohmann::json SerializeClimate(const BiomeClimate& climate)
		{
			return nlohmann::json
			{
				{ TEMPERATURE_KEY,     SerializeRange(climate.temperature) },
				{ MOISTURE_KEY,        SerializeRange(climate.moisture) },
				{ CONTINENTALNESS_KEY, SerializeRange(climate.continentalness) },
				{ EROSION_KEY,         SerializeRange(climate.erosion) }
			};
		}

		BiomeClimate DeserializeClimate(const nlohmann::json& json)
		{
			return BiomeClimate
			{
				.temperature     = DeserializeRange(json, TEMPERATURE_KEY),
				.moisture        = DeserializeRange(json, MOISTURE_KEY),
				.continentalness = DeserializeRange(json, CONTINENTALNESS_KEY),
				.erosion         = DeserializeRange(json, EROSION_KEY)
			};
		}

		bool IsOrdered(const BiomeClimate& climate)
		{
			return climate.temperature.min     <= climate.temperature.max
			    && climate.moisture.min        <= climate.moisture.max
			    && climate.continentalness.min <= climate.continentalness.max
			    && climate.erosion.min         <= climate.erosion.max;
		}

		nlohmann::json SerializeColor(const glm::vec3& color)
		{
			return nlohmann::json { color.r, color.g, color.b };
		}

		glm::vec3 DeserializeColor(const nlohmann::json& json)
		{
			return glm::vec3(json.at(0).get<float>(), json.at(1).get<float>(), json.at(2).get<float>());
		}

		nlohmann::json SerializeTerrain(const BiomeTerrain& terrain)
		{
			return nlohmann::json
			{
				{ "baseHeight", terrain.baseHeight },
				{ "amplitude",  terrain.amplitude },
				{ "frequency",  terrain.frequency },
				{ "octaves",    terrain.octaves },
				{ "ridged",     terrain.ridged }
			};
		}

		BiomeTerrain DeserializeTerrain(const nlohmann::json& json)
		{
			return BiomeTerrain
			{
				.baseHeight = json.at("baseHeight").get<float>(),
				.amplitude  = json.at("amplitude").get<float>(),
				.frequency  = json.at("frequency").get<float>(),
				.octaves    = json.at("octaves").get<int32_t>(),
				.ridged     = json.at("ridged").get<bool>()
			};
		}

		nlohmann::json SerializeColors(const BiomeColors& colors)
		{
			return nlohmann::json
			{
				{ "low",            SerializeColor(colors.lowColor) },
				{ "high",           SerializeColor(colors.highColor) },
				{ "rock",           SerializeColor(colors.rockColor) },
				{ "rockSlopeStart", colors.rockSlopeStart },
				{ "rockSlopeEnd",   colors.rockSlopeEnd }
			};
		}

		BiomeColors DeserializeColors(const nlohmann::json& json)
		{
			return BiomeColors
			{
				.lowColor       = DeserializeColor(json.at("low")),
				.highColor      = DeserializeColor(json.at("high")),
				.rockColor      = DeserializeColor(json.at("rock")),
				.rockSlopeStart = json.at("rockSlopeStart").get<float>(),
				.rockSlopeEnd   = json.at("rockSlopeEnd").get<float>()
			};
		}
	}

	float ClimateRange::distanceTo(float value) const
	{
		return std::max(0.f, min - value) + std::max(0.f, value - max);
	}

	float Biome::distanceTo(const Climate& other) const
	{
		const glm::vec4 offset = glm::vec4(climate.temperature.distanceTo(other.temperature),
		                                   climate.moisture.distanceTo(other.moisture),
		                                   climate.continentalness.distanceTo(other.continentalness),
		                                   climate.erosion.distanceTo(other.erosion));

		return glm::dot(offset, offset);
	}

	nlohmann::json Biome::Serialize(const Biome& biome)
	{
		return nlohmann::json
		{
			{ "name",    biome.name },
			{ "climate", SerializeClimate(biome.climate) },
			{ "terrain", SerializeTerrain(biome.terrain) },
			{ "colors",  SerializeColors(biome.colors) }
		};
	}

	std::optional<Biome> Biome::Deserialize(const nlohmann::json& json)
	{
		const Biome biome =
		{
			.name    = json.at("name").get<std::string>(),
			.climate = DeserializeClimate(json.at("climate")),
			.terrain = DeserializeTerrain(json.at("terrain")),
			.colors  = DeserializeColors(json.at("colors"))
		};

		if (!IsOrdered(biome.climate))
		{
			Fs::ReportMalformedJson("climate ranges need their minimum first");
			return std::nullopt;
		}

		return biome;
	}

	const std::vector<Biome>& BiomeLibrary::getBiomes() const
	{
		return biomes;
	}

	std::vector<std::string> BiomeLibrary::getNames() const
	{
		std::vector<std::string> names;
		for (const Biome& biome : biomes)
			names.push_back(biome.name);

		return names;
	}

	const Biome& BiomeLibrary::get(BiomeIndex index) const
	{
		return biomes[index];
	}

	BiomeIndex BiomeLibrary::getDefault() const
	{
		return defaultBiome;
	}

	std::optional<BiomeIndex> BiomeLibrary::indexOf(std::string_view name) const
	{
		const auto found = std::ranges::find(biomes, name, &Biome::name);
		if (found == biomes.end())
			return std::nullopt;

		return static_cast<BiomeIndex>(found - biomes.begin());
	}

	BiomeIndex BiomeLibrary::select(const Climate& climate, uint64_t tie_breaker) const
	{
		const auto distance_to = [&climate](const Biome& biome) { return biome.distanceTo(climate); };
		const float nearest    = std::ranges::min(biomes | std::views::transform(distance_to));

		auto candidates = std::views::iota(size_t { 0 }, biomes.size())
		                | std::views::filter([&](size_t index) { return distance_to(biomes[index]) == nearest; });

		const auto candidate_count = static_cast<uint64_t>(std::ranges::distance(candidates));
		return static_cast<BiomeIndex>(*std::ranges::next(candidates.begin(), static_cast<std::ptrdiff_t>(tie_breaker % candidate_count)));
	}

	std::optional<BiomeLibrary> BiomeLibrary::Create(std::vector<Biome> biomes, std::string_view default_biome)
	{
		std::vector<std::string_view> names;
		for (const Biome& biome : biomes)
			names.push_back(biome.name);

		std::ranges::sort(names);
		if (names.empty() || names.front().empty() || std::ranges::adjacent_find(names) != names.end())
		{
			Fs::ReportMalformedJson("biomes need unique, non-empty names");
			return std::nullopt;
		}

		BiomeLibrary library;
		library.biomes = std::move(biomes);

		const std::optional<BiomeIndex> default_index = library.indexOf(default_biome);
		if (!default_index.has_value())
		{
			Fs::ReportMalformedJson("the default biome is not in the library");
			return std::nullopt;
		}

		library.defaultBiome = *default_index;
		return library;
	}

	nlohmann::json BiomeLibrary::Serialize(const BiomeLibrary& library)
	{
		nlohmann::json biomes = nlohmann::json::array();
		for (const Biome& biome : library.biomes)
			biomes.push_back(Biome::Serialize(biome));

		return nlohmann::json
		{
			{ "formatVersion", BIOME_FORMAT_VERSION },
			{ "default",       library.get(library.defaultBiome).name },
			{ "biomes",        std::move(biomes) }
		};
	}

	std::optional<BiomeLibrary> BiomeLibrary::Deserialize(const nlohmann::json& json)
	{
		if (json.at("formatVersion").get<uint32_t>() != BIOME_FORMAT_VERSION)
			return std::nullopt;

		std::vector<Biome> biomes;
		for (const nlohmann::json& entry : json.at("biomes"))
		{
			const std::optional<Biome> biome = Biome::Deserialize(entry);
			if (!biome.has_value())
				return std::nullopt;

			biomes.push_back(*biome);
		}

		return Create(std::move(biomes), json.at("default").get<std::string>());
	}
}
