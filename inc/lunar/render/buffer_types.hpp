#pragma once
#include <lunar/api.hpp>
#include <cstddef>
#include <cstdint>

namespace lunar::Render
{
	enum class LUNAR_API BufferUsageFlagBits : uint32_t
	{
		eUnknown     = 0,
		eVertex      = 1 << 0,
		eIndex       = 1 << 1,
		eUniform     = 1 << 2,
		eStorage     = 1 << 3,
		eIndirect    = 1 << 4,
		eTransferSrc = 1 << 5,
		eTransferDst = 1 << 6
	};

	LUNAR_FLAGS(BufferUsageFlags, BufferUsageFlagBits);

	enum class LUNAR_API MemoryLocation
	{
		eGpuOnly,
		eUpload,
		eReadback
	};

	struct LUNAR_API BufferDesc
	{
		size_t           size     = 0;
		BufferUsageFlags usage    = {};
		MemoryLocation   location = MemoryLocation::eGpuOnly;
	};
}
