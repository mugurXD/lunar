#pragma once
#include <lunar/api.hpp>
#include <lunar/render/frustum.hpp>
#include <lunar/render/instance_draw_table.hpp>
#include <lunar/render/render_device.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace lunar::Render
{
	constexpr std::string_view INSTANCE_CULL_SHADER = "instance_cull.comp";

	struct LUNAR_API InstanceCullOutput
	{
		uint64_t commandAddress = 0;
		uint64_t visibleAddress = 0;
	};

	class LUNAR_API VisibleInstanceBuffers
	{
	public:
		explicit VisibleInstanceBuffers(RenderDevice& device) noexcept;
		~VisibleInstanceBuffers() noexcept;

		VisibleInstanceBuffers(const VisibleInstanceBuffers&)            = delete;
		VisibleInstanceBuffers& operator=(const VisibleInstanceBuffers&) = delete;

		uint64_t reserve(const Frame& frame, uint32_t instance_count);

	private:
		struct FrameBuffer
		{
			BufferHandle buffer = {};
			size_t       size   = 0;
		};

		RenderDevice&            device;
		std::vector<FrameBuffer> buffers;
	};

	LUNAR_API void RecordInstanceCull(Frame&                    frame,
	                                  const InstanceDrawTable&  table,
	                                  const Frustum&            frustum,
	                                  const glm::vec3&          eye,
	                                  const InstanceCullOutput& output);
}
