#pragma once
#include <lunar/utils/hash.hpp>

#include <cstdint>

namespace trok
{
	using lunar::GOLDEN_GAMMA;
	using lunar::MixHash;

	constexpr int32_t SeedFromHash(uint64_t hash)
	{
		return static_cast<int32_t>(static_cast<uint32_t>(hash));
	}
}
