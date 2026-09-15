#pragma once
#include <lunar/api.hpp>
#include <lunar/render/gpu_types.hpp>

#include <cstdint>

namespace lunar::Render
{
	enum class LUNAR_API ImageUsageFlagBits : uint32_t
	{
		eUnknown         = 0,
		eColorAttachment = 1 << 0,
		eDepthAttachment = 1 << 1,
		eSampled         = 1 << 2,
		eStorage         = 1 << 3,
		eTransferSrc     = 1 << 4,
		eTransferDst     = 1 << 5
	};

	LUNAR_FLAGS(ImageUsageFlags, ImageUsageFlagBits);

	struct LUNAR_API ImageDesc
	{
		Extent2D        extent      = {};
		Format          format      = Format::eUndefined;
		ImageUsageFlags usage       = {};
		uint32_t        mipLevels   = 1;
		uint32_t        arrayLayers = 1;
	};
}
