#version 460
#include "common/textures.glsl"

// This is the highlight compression of Khronos PBR Neutral without its toe: the toe offsets the 4% specular reflection of
// dielectrics, which lambert-only shading never adds, so leaving it out keeps every base color lit by a unit white light as authored.
const float START_COMPRESSION = 0.8;
const float SHOULDER_LENGTH   = 1.0 - START_COMPRESSION;
const float DESATURATION      = 0.15;
const float OPAQUE            = 1.0;

layout(push_constant) uniform TonemapConstants
{
	uint  sceneTexture;
	float exposure;
} constants;

layout(location = 0) out vec4 outColor;

vec3 CompressHighlights(vec3 color)
{
	const float peak = max(color.r, max(color.g, color.b));
	if (peak < START_COMPRESSION)
		return color;

	const float new_peak     = 1.0 - SHOULDER_LENGTH * SHOULDER_LENGTH / (peak + SHOULDER_LENGTH - START_COMPRESSION);
	const float desaturation = 1.0 - 1.0 / (DESATURATION * (peak - new_peak) + 1.0);
	return mix(color * (new_peak / peak), vec3(new_peak), desaturation);
}

void main()
{
	const vec3 scene = FetchTexel(constants.sceneTexture, ivec2(gl_FragCoord.xy)).rgb;
	outColor = vec4(CompressHighlights(scene * constants.exposure), OPAQUE);
}
