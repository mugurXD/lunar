#version 460
#include "common/mesh_interface.glsl"
#include "common/lighting.glsl"

layout(location = 1) flat in vec4 inColor;
layout(location = 2)      in vec3 inWorldPosition;
layout(location = 3) flat in vec3 inFacetNormal;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(ShadeSurface(inColor.rgb, normalize(inFacetNormal), inWorldPosition), inColor.a);
}
