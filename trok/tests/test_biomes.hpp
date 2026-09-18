#pragma once
#include <trok/world/biome.hpp>

#include <string_view>
#include <vector>

namespace
{
	constexpr trok::BiomeIndex FLAT_BIOME   = 0;
	constexpr trok::BiomeIndex TALL_BIOME   = 1;
	constexpr std::string_view UNKNOWN_NAME = "test:unknown";

	const trok::Biome FLAT =
	{
		.name    = "test:flat",
		.climate = { .temperature = { 0.f, 1.f }, .continentalness = { -1.f, 0.f } },
		.terrain = { .heightOffset = 0.f, .amplitude = 5.f, .frequency = 0.01f, .octaves = 2 },
		.colors  = { .lowColor = { 0.1f, 0.5f, 0.1f }, .highColor = { 0.2f, 0.6f, 0.2f }, .rockColor = { 0.3f, 0.3f, 0.3f } }
	};

	const trok::Biome TALL =
	{
		.name    = "test:tall",
		.climate = { .continentalness = { 0.5f, 1.f } },
		.terrain = { .heightOffset = 200.f, .amplitude = 20.f, .frequency = 0.01f, .octaves = 2, .ridged = true },
		.colors  = { .lowColor = { 0.5f, 0.5f, 0.6f }, .highColor = { 0.9f, 0.9f, 1.f }, .rockColor = { 0.4f, 0.4f, 0.4f } }
	};

	trok::BiomeLibrary TestBiomes()
	{
		return *trok::BiomeLibrary::Create({ FLAT, TALL }, FLAT.name);
	}
}
