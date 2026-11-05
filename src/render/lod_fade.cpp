#include <lunar/render/lod_fade.hpp>

#include <limits>

namespace lunar::Render
{
	namespace
	{
		const glm::vec2 ALWAYS_FADED_IN = glm::vec2(-std::numeric_limits<float>::infinity());

		bool Overlaps(const glm::vec2& range, float nearest, float farthest)
		{
			return nearest <= range.y && farthest >= range.x;
		}
	}

	std::array<LodDraw, MAX_INSTANCE_LODS> SelectLodDraws(const std::array<InstanceLod, MAX_INSTANCE_LODS>& lods, float nearest, float farthest)
	{
		std::array<LodDraw, MAX_INSTANCE_LODS> draws   = {};
		glm::vec2                              fade_in = ALWAYS_FADED_IN;

		for (size_t index = 0; index < lods.size(); index++)
		{
			if (lods[index].mesh == MeshHandle {})
				continue;

			const glm::vec2 fade_out = { lods[index].maxDistance - lods[index].fadeLength, lods[index].maxDistance };
			if (Overlaps({ fade_in.x, fade_out.y }, nearest, farthest))
				draws[index] = { lods[index].mesh, glm::vec4(fade_in, fade_out), Overlaps(fade_in, nearest, farthest) || Overlaps(fade_out, nearest, farthest) };

			fade_in = fade_out;
		}

		return draws;
	}
}
