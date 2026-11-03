#pragma once
#include <lunar/utils/hash.hpp>

#include <cstdint>

namespace trok
{
	using lunar::GOLDEN_GAMMA;
	using lunar::MixHash;

	constexpr int    UNIT_HASH_SHIFT = 11;
	constexpr double UNIT_HASH_SCALE = 0x1p-53;

	constexpr int32_t SeedFromHash(uint64_t hash)
	{
		return static_cast<int32_t>(static_cast<uint32_t>(hash));
	}

	constexpr double UnitFromHash(uint64_t hash)
	{
		return static_cast<double>(hash >> UNIT_HASH_SHIFT) * UNIT_HASH_SCALE;
	}
}
