#pragma once
#include <lunar/api.hpp>
#include <lunar/render/gpu_types.hpp>

#include <glm/glm.hpp>

#include <span>

namespace lunar::Render
{
	enum class LUNAR_API LoadOp
	{
		eLoad,
		eClear
	};

	struct LUNAR_API ColorAttachment
	{
		ImageHandle image      = {};
		LoadOp      loadOp     = LoadOp::eClear;
		glm::vec4   clearColor = {};
	};

	struct LUNAR_API RenderingDesc
	{
		std::span<const ColorAttachment> colorAttachments = {};
	};

	class LUNAR_API CommandList
	{
	public:
		CommandList()          noexcept = default;
		virtual ~CommandList() noexcept = default;

		CommandList(const CommandList&)            = delete;
		CommandList& operator=(const CommandList&) = delete;

		virtual void beginRendering(const RenderingDesc& desc) = 0;
		virtual void endRendering()                            = 0;
	};
}
