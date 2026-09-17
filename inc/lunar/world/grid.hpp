#pragma once
#include <lunar/api.hpp>

#include <cstddef>
#include <cstdint>

namespace lunar::World
{
	template<typename Tag>
	struct GridCoord
	{
		int32_t x = 0;
		int32_t z = 0;

		bool operator==(const GridCoord&) const = default;
	};

	struct ChunkTag;
	struct RegionTag;

	using ChunkCoord  = GridCoord<ChunkTag>;
	using RegionCoord = GridCoord<RegionTag>;

	LUNAR_API size_t HashGridCoord(int32_t x, int32_t z);

	struct LUNAR_API GridCoordHash
	{
		template<typename Tag>
		size_t operator()(const GridCoord<Tag>& coord) const
		{
			return HashGridCoord(coord.x, coord.z);
		}
	};

	constexpr int32_t FloorDivide(int32_t value, int32_t divisor)
	{
		const int32_t quotient = value / divisor;
		return (value % divisor != 0 && (value < 0) != (divisor < 0)) ? quotient - 1 : quotient;
	}

	template<typename Tag>
	int64_t DistanceSquared(GridCoord<Tag> a, GridCoord<Tag> b)
	{
		const int64_t delta_x = static_cast<int64_t>(a.x) - b.x;
		const int64_t delta_z = static_cast<int64_t>(a.z) - b.z;
		return delta_x * delta_x + delta_z * delta_z;
	}

	template<typename Tag>
	bool IsWithin(GridCoord<Tag> coord, GridCoord<Tag> center, int32_t radius)
	{
		return DistanceSquared(coord, center) <= static_cast<int64_t>(radius) * radius;
	}
}
