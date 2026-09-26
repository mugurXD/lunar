#version 460
#include "common/ui_interface.glsl"
#include "common/textures.glsl"

const uint  NO_TEXTURE     = 0xFFFFFFFFu;
const float SRGB_THRESHOLD = 0.04045;
const float SRGB_LINEAR    = 12.92;
const float SRGB_OFFSET    = 0.055;
const float SRGB_SCALE     = 1.055;
const float SRGB_GAMMA     = 2.4;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

vec3 SrgbToLinear(vec3 color)
{
	const vec3 low  = color / SRGB_LINEAR;
	const vec3 high = pow((color + SRGB_OFFSET) / SRGB_SCALE, vec3(SRGB_GAMMA));
	return mix(high, low, lessThanEqual(color, vec3(SRGB_THRESHOLD)));
}

void main()
{
	vec4 color = inColor;
	if (constants.texture != NO_TEXTURE)
		color *= SampleTexture(constants.texture, SAMPLER_LINEAR_CLAMP, inUv);

	if (constants.linearOutput != 0 && color.a > 0.0)
		color.rgb = SrgbToLinear(color.rgb / color.a) * color.a;

	outColor = color;
}
