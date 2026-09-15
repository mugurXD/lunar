#pragma once
#include <lunar/api.hpp>
#include <lunar/render/gpu_types.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace lunar::Render
{
	constexpr size_t MAX_PUSH_CONSTANTS_SIZE = 128;

	enum class LUNAR_API LoadOp
	{
		eLoad,
		eClear
	};

	enum class LUNAR_API IndexType
	{
		eUint16,
		eUint32
	};

	struct LUNAR_API ColorAttachment
	{
		ImageHandle image      = {};
		LoadOp      loadOp     = LoadOp::eClear;
		glm::vec4   clearColor = {};
	};

	struct LUNAR_API DepthAttachment
	{
		ImageHandle image      = {};
		LoadOp      loadOp     = LoadOp::eClear;
		float       clearDepth = 0.f;
	};

	struct LUNAR_API RenderingDesc
	{
		std::span<const ColorAttachment> colorAttachments = {};
		std::optional<DepthAttachment>   depthAttachment  = std::nullopt;
	};

	class LUNAR_API CommandList
	{
	public:
		CommandList()          noexcept = default;
		virtual ~CommandList() noexcept = default;

		CommandList(const CommandList&)            = delete;
		CommandList& operator=(const CommandList&) = delete;

		virtual void beginRendering(const RenderingDesc& desc)                                  = 0;
		virtual void endRendering()                                                             = 0;
		virtual void bindPipeline(PipelineHandle pipeline)                                      = 0;
		virtual void pushConstants(std::span<const std::byte> data)                             = 0;
		virtual void bindIndexBuffer(BufferHandle buffer, size_t offset, IndexType index_type) = 0;
		virtual void memoryBarrier()                                                            = 0;

		virtual void draw(uint32_t vertex_count,
		                  uint32_t instance_count = 1,
		                  uint32_t first_vertex   = 0,
		                  uint32_t first_instance = 0) = 0;

		virtual void drawIndexed(uint32_t index_count,
		                         uint32_t instance_count = 1,
		                         uint32_t first_index    = 0,
		                         int32_t  vertex_offset  = 0,
		                         uint32_t first_instance = 0) = 0;

		virtual void dispatch(uint32_t group_count_x,
		                      uint32_t group_count_y = 1,
		                      uint32_t group_count_z = 1) = 0;

		template<typename T>
		void pushConstants(const T& constants)
		{
			static_assert(std::is_trivially_copyable_v<T>, "Push constants must be trivially copyable");
			static_assert(sizeof(T) <= MAX_PUSH_CONSTANTS_SIZE, "Push constants exceed the guaranteed size");
			pushConstants(std::as_bytes(std::span(&constants, 1)));
		}
	};
}
