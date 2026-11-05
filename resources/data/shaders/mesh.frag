#version 460
#include "common/mesh_interface.glsl"
#include "common/lighting.glsl"

layout(location = 0)      in vec3 inNormal;
layout(location = 1) flat in vec4 inColor;
layout(location = 2)      in vec3 inWorldPosition;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(ShadeSurface(inColor.rgb, FaceNormal(inWorldPosition, inNormal), inWorldPosition), inColor.a);
}
