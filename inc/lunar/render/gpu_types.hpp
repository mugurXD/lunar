#pragma once
#include <lunar/api.hpp>
#include <cstdint>

namespace lunar::Render
{
	template<typename Tag>
	struct GpuHandle
	{
		uint32_t index      = 0;
		uint32_t generation = 0;

		bool operator==(const GpuHandle&) const = default;
	};

	struct BufferTag;
	struct ImageTag;

	using BufferHandle = GpuHandle<BufferTag>;
	using ImageHandle  = GpuHandle<ImageTag>;

	struct LUNAR_API UploadTicket
	{
		uint64_t value = 0;
	};
}
