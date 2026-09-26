#pragma once
#include <lunar/api.hpp>
#include <lunar/render/gpu_types.hpp>

#include <cstdint>
#include <limits>

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

	enum class LUNAR_API SamplerType : uint32_t
	{
		eLinearClamp,
		eLinearRepeat,
		eNearestClamp,
		eNearestRepeat,
		eAnisotropicRepeat,
		eCount
	};

	constexpr uint32_t MAX_TEXTURES          = 4096;
	constexpr uint32_t SAMPLER_COUNT         = static_cast<uint32_t>(SamplerType::eCount);
	constexpr uint32_t INVALID_TEXTURE_INDEX = std::numeric_limits<uint32_t>::max();

	struct LUNAR_API ImageDesc
	{
		Extent2D        extent      = {};
		Format          format      = Format::eUndefined;
		ImageUsageFlags usage       = {};
		uint32_t        mipLevels   = 1;
		uint32_t        arrayLayers = 1;
	};
}
