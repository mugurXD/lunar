#include <lunar/render/instance_cull.hpp>

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
	}

	void RecordInstanceCull(Frame&                    frame,
	                        const InstanceDrawTable&  table,
	                        const Frustum&            frustum,
	                        const glm::vec3&          eye,
	                        const InstanceCullOutput& output)
	{
		if (table.instanceCount == 0)
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
}
