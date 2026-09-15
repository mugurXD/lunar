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
	struct PipelineTag;

	using BufferHandle   = GpuHandle<BufferTag>;
	using ImageHandle    = GpuHandle<ImageTag>;
	using PipelineHandle = GpuHandle<PipelineTag>;

	enum class LUNAR_API Format
	{
		eUndefined,
		eRGBA8Unorm,
		eRGBA8Srgb,
		eBGRA8Unorm,
		eBGRA8Srgb,
		eRGBA16Float,
		eRGBA32Float,
		eD32Float
	};

	struct LUNAR_API UploadTicket
	{
		uint64_t value = 0;
	};
}
