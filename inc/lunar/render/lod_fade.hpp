#pragma once
#include <lunar/api.hpp>
#include <lunar/render/components.hpp>

#include <glm/glm.hpp>

#include <array>

namespace lunar::Render
{
	struct LUNAR_API LodDraw
	{
		MeshHandle mesh      = {};
		glm::vec4  fadeBands = {};
		bool       fading    = false;
	};

	LUNAR_API std::array<LodDraw, MAX_INSTANCE_LODS> SelectLodDraws(const std::array<InstanceLod, MAX_INSTANCE_LODS>& lods, float nearest, float farthest);
}
