#include <lunar/render/instance_draw_table.hpp>
#include <lunar/debug.hpp>

#include <algorithm>
#include <iterator>
#include <tuple>

namespace lunar::Render
{
	namespace
	{
		constexpr float HALF = 0.5f;

		auto SlotKey(const InstanceDraw& draw)
		{
			return std::tuple(draw.fading, reinterpret_cast<std::uintptr_t>(draw.mesh), draw.fadeBands.x, draw.fadeBands.y, draw.fadeBands.z, draw.fadeBands.w);
		}

		glm::vec4 BoundingSphere(const Bounds& bounds)
		{
			return glm::vec4((bounds.min + bounds.max) * HALF, glm::distance(bounds.min, bounds.max) * HALF);
		}
	}

	InstanceDrawTable BuildInstanceDrawTable(std::span<const InstanceDraw> draws)
	{
		std::vector<InstanceDraw> sorted(draws.begin(), draws.end());
		std::ranges::stable_sort(sorted, {}, SlotKey);

		InstanceDrawTable table;
		table.draws.reserve(sorted.size());

		const InstanceDraw* previous = nullptr;
		for (const InstanceDraw& draw : sorted)
		{
			DEBUG_ASSERT(glm::mat3(draw.transform) == glm::mat3(1.f), "Visible instances can only fold a translation into their position");

			if (previous == nullptr || SlotKey(*previous) != SlotKey(draw))
				table.slots.push_back({ draw.mesh, draw.fadeBands, draw.fading, table.instanceCount });

			table.draws.push_back({
				.transform       = draw.transform,
				.fadeBands       = draw.fadeBands,
				.boundingSphere  = BoundingSphere(draw.mesh->bounds),
				.instanceAddress = draw.instanceAddress,
				.instanceCount   = draw.instanceCount,
				.outputOffset    = table.instanceCount,
				.slot            = static_cast<uint32_t>(table.slots.size() - 1)
			});

			table.instanceCount += draw.instanceCount;
			previous             = &draw;
		}

		return table;
	}

	std::vector<DrawIndexedIndirectCommand> BuildInstanceDrawCommands(const InstanceDrawTable& table)
	{
		std::vector<DrawIndexedIndirectCommand> commands;
		commands.reserve(table.slots.size());

		std::ranges::transform(table.slots, std::back_inserter(commands), [](const InstanceDrawSlot& slot) {
			return DrawIndexedIndirectCommand { .indexCount = slot.mesh->indexCount, .firstInstance = slot.firstInstance };
		});

		return commands;
	}
}
