#version 460
#include "common/mesh_interface.glsl"
#include "common/lighting.glsl"

layout(location = 0)      in vec3 inNormal;
layout(location = 1) flat in vec3 inColor;
layout(location = 2)      in vec3 inWorldPosition;

layout(location = 0) out vec4 outColor;

void main()
{
	const vec3 face_normal = normalize(cross(dFdx(inWorldPosition), dFdy(inWorldPosition)));
	const vec3 normal      = dot(face_normal, inNormal) < 0.0 ? -face_normal : face_normal;

	outColor = vec4(ShadeSurface(inColor, normal, inWorldPosition), 1.0);
}
