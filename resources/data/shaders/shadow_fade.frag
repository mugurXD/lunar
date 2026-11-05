#version 460
#include "common/lod_fade.glsl"

layout(location = 3) flat in vec2 inFade;

layout(location = 0) out float outDepth;

void main()
{
	ApplyLodFade(inFade);
	outDepth = gl_FragCoord.z;
}
