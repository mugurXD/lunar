#pragma once
#include <lunar/api.hpp>
#include <lunar/render/gpu_types.hpp>

#include <cstddef>
#include <span>

namespace lunar::Render
{
	enum class LUNAR_API PrimitiveTopology
	{
		eTriangleList,
		eTriangleStrip,
		eLineList,
		ePointList
	};

	enum class LUNAR_API PolygonMode
	{
		eFill,
		eLine
	};

	enum class LUNAR_API CullMode
	{
		eNone,
		eFront,
		eBack
	};

	enum class LUNAR_API FrontFace
	{
		eCounterClockwise,
		eClockwise
	};

	enum class LUNAR_API CompareOp
	{
		eLess,
		eLessOrEqual,
		eGreater,
		eGreaterOrEqual,
		eEqual,
		eAlways
	};

	enum class LUNAR_API BlendMode
	{
		eOpaque,
		eAlpha,
		eAdditive
	};

	struct LUNAR_API GraphicsPipelineDesc
	{
		std::span<const std::byte> vertexShader   = {};
		std::span<const std::byte> fragmentShader = {};
		std::span<const Format>    colorFormats   = {};
		Format                     depthFormat    = Format::eUndefined;
		PrimitiveTopology          topology       = PrimitiveTopology::eTriangleList;
		PolygonMode                polygonMode    = PolygonMode::eFill;
		CullMode                   cullMode       = CullMode::eNone;
		FrontFace                  frontFace      = FrontFace::eCounterClockwise;
		bool                       depthTest      = false;
		bool                       depthWrite     = false;
		CompareOp                  depthCompare   = CompareOp::eLess;
		BlendMode                  blendMode      = BlendMode::eOpaque;
	};

	struct LUNAR_API ComputePipelineDesc
	{
		std::span<const std::byte> computeShader = {};
	};
}
