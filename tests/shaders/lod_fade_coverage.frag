#version 460
#include "common/lod_fade.glsl"

layout(push_constant) uniform Constants
{
	vec2 fade;
} constants;

layout(location = 0) out vec4 outColor;

void main()
{
	ApplyLodFade(constants.fade);
	outColor = vec4(1.0);
}
