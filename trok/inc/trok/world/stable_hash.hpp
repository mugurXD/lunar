#pragma once
#include <cstdint>

namespace trok
{
	constexpr uint64_t GOLDEN_GAMMA     = 0x9E3779B97F4A7C15ull;
	constexpr uint64_t MIX_MULTIPLIER_A = 0xBF58476D1CE4E5B9ull;
	constexpr uint64_t MIX_MULTIPLIER_B = 0x94D049BB133111EBull;

	constexpr uint64_t MixHash(uint64_t hash, uint64_t value)
	{
		uint64_t mixed = hash + GOLDEN_GAMMA + value;
		mixed = (mixed ^ (mixed >> 30)) * MIX_MULTIPLIER_A;
		mixed = (mixed ^ (mixed >> 27)) * MIX_MULTIPLIER_B;
		return mixed ^ (mixed >> 31);
	}

	constexpr int32_t SeedFromHash(uint64_t hash)
	{
		return static_cast<int32_t>(static_cast<uint32_t>(hash));
	}
}
