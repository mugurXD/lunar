#include <lunar/render/instance_cull.hpp>
#include <lunar/render/common.hpp>

#include <bit>
#include <span>

namespace lunar::Render
{
	namespace
	{
		constexpr uint32_t CULL_GROUP_SIZE = 64;

		struct CullView
		{
			FrustumPlanes planes = {};
			glm::vec4     eye    = {};
		};

		struct CullConstants
		{
			uint64_t viewAddress    = 0;
			uint64_t drawAddress    = 0;
			uint64_t commandAddress = 0;
			uint64_t visibleAddress = 0;
			uint32_t drawCount      = 0;
			uint32_t instanceCount  = 0;
		};

		struct VisibleInstanceConstants
		{
			uint64_t                               sceneAddress    = 0;
			uint64_t                               instanceAddress = 0;
			uint64_t                               vertexAddress   = 0;
			alignas(STD430_VECTOR_ALIGN) glm::vec4 fadeBands       = {};
		};
	}

	VisibleInstanceBuffers::VisibleInstanceBuffers(RenderDevice& device) noexcept
		: device(device)
	{
	}

	VisibleInstanceBuffers::~VisibleInstanceBuffers() noexcept
	{
		for (const FrameBuffer& frame_buffer : buffers)
			device.destroyBuffer(frame_buffer.buffer);
	}

	uint64_t VisibleInstanceBuffers::reserve(const Frame& frame, uint32_t instance_count)
	{
		if (frame.getIndex() >= buffers.size())
			buffers.resize(frame.getIndex() + 1);

		FrameBuffer& frame_buffer = buffers[frame.getIndex()];
		const size_t required     = instance_count * sizeof(InstanceData);
		if (required > frame_buffer.size)
		{
			const size_t size = std::bit_ceil(required);

			device.destroyBuffer(frame_buffer.buffer);
			frame_buffer.buffer = device.createBuffer({ size, BufferUsageFlags(BufferUsageFlagBits::eStorage), MemoryLocation::eGpuOnly }, {});
			frame_buffer.size   = frame_buffer.buffer != BufferHandle {} ? size : 0;
		}

		return device.getBufferAddress(frame_buffer.buffer);
	}

	void RecordInstanceCull(Frame&                    frame,
	                        const InstanceDrawTable&  table,
	                        const Frustum&            frustum,
	                        const glm::vec3&          eye,
	                        const InstanceCullOutput& output)
	{
		if (table.instanceCount == 0 || output.commandAddress == 0 || output.visibleAddress == 0)
			return;

		const uint64_t view_address = frame.writeTransient(CullView { frustum.getPlanes(), glm::vec4(eye, 1.f) });
		const uint64_t draw_address = frame.writeTransient(std::span<const GpuInstanceDraw>(table.draws));
		if (view_address == 0 || draw_address == 0)
			return;

		CommandList& commands = frame.commandList();
		commands.pushConstants(CullConstants {
			.viewAddress    = view_address,
			.drawAddress    = draw_address,
			.commandAddress = output.commandAddress,
			.visibleAddress = output.visibleAddress,
			.drawCount      = static_cast<uint32_t>(table.draws.size()),
			.instanceCount  = table.instanceCount
		});
		commands.dispatch((table.instanceCount + CULL_GROUP_SIZE - 1) / CULL_GROUP_SIZE);
	}

	void RecordInstanceDraws(Frame& frame, std::span<const InstanceDrawSlot> slots, const InstanceDrawInput& input)
	{
		CommandList& commands = frame.commandList();
		for (size_t index = 0; index < slots.size(); index++)
		{
			const InstanceDrawSlot& slot = slots[index];
			if (index == 0 || slots[index - 1].fading != slot.fading)
				commands.bindPipeline(slot.fading ? input.fadingPipeline : input.steadyPipeline);

			commands.pushConstants(VisibleInstanceConstants { input.sceneAddress, input.visibleAddress, slot.mesh->vertexAddress, slot.fadeBands });
			commands.bindIndexBuffer(slot.mesh->indexBuffer, 0, IndexType::eUint32);
			commands.drawIndexedIndirect(input.commandBuffer, input.commandOffset + index * sizeof(DrawIndexedIndirectCommand), 1);
		}
	}
}
