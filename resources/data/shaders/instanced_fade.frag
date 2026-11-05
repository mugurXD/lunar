#version 460
#include "common/instance_interface.glsl"
#include "common/lighting.glsl"
#include "common/lod_fade.glsl"

layout(location = 0)      in vec3 inNormal;
layout(location = 1) flat in vec4 inColor;
layout(location = 2)      in vec3 inWorldPosition;
layout(location = 3) flat in vec2 inFade;

layout(location = 0) out vec4 outColor;

void main()
{
	ApplyLodFade(inFade);
	outColor = vec4(ShadeSurface(inColor.rgb, FaceNormal(inWorldPosition, inNormal), inWorldPosition), inColor.a);
}
