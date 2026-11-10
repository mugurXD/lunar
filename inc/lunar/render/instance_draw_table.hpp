#pragma once
#include <lunar/api.hpp>
#include <lunar/render/mesh_registry.hpp>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace lunar::Render
{
	constexpr size_t STD430_MATRIX_ALIGN = 16;

	struct LUNAR_API InstanceDraw
	{
		const Mesh* mesh            = nullptr;
		uint64_t    instanceAddress = 0;
		uint32_t    instanceCount   = 0;
		glm::mat4   transform       = glm::mat4(1.f);
		glm::vec4   fadeBands       = {};
		bool        fading          = false;
	};

	struct alignas(STD430_MATRIX_ALIGN) LUNAR_API GpuInstanceDraw
	{
		glm::mat4 transform       = glm::mat4(1.f);
		glm::vec4 fadeBands       = {};
		glm::vec4 boundingSphere  = {};
		uint64_t  instanceAddress = 0;
		uint32_t  instanceCount   = 0;
		uint32_t  outputOffset    = 0;
		uint32_t  slot            = 0;
	};

	struct LUNAR_API InstanceDrawSlot
	{
		const Mesh* mesh          = nullptr;
		glm::vec4   fadeBands     = {};
		bool        fading        = false;
		uint32_t    firstInstance = 0;
	};

	struct LUNAR_API InstanceDrawTable
	{
		std::vector<GpuInstanceDraw>  draws         = {};
		std::vector<InstanceDrawSlot> slots         = {};
		uint32_t                      instanceCount = 0;
	};

	LUNAR_API InstanceDrawTable BuildInstanceDrawTable(std::span<const InstanceDraw> draws);
}
