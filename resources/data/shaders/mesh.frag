#version 460
#include "common/mesh_interface.glsl"

layout(location = 0)      in vec3 inNormal;
layout(location = 1) flat in vec3 inColor;
layout(location = 2)      in vec3 inWorldPosition;

layout(location = 0) out vec4 outColor;

void main()
{
	const vec3  face_normal = normalize(cross(dFdx(inWorldPosition), dFdy(inWorldPosition)));
	const vec3  normal      = dot(face_normal, inNormal) < 0.0 ? -face_normal : face_normal;
	const float diffuse     = max(dot(normal, -constants.scene.lightDirection.xyz), 0.0);
	const vec3  lighting    = constants.scene.ambientColor.rgb + constants.scene.lightColor.rgb * diffuse;
	const vec2  fog_range   = constants.scene.fogRange.xy;
	const float distance    = length(inWorldPosition - constants.scene.cameraPosition.xyz);
	const float fog         = fog_range.y > fog_range.x ? smoothstep(fog_range.x, fog_range.y, distance) : 0.0;

	outColor = vec4(mix(inColor * lighting, constants.scene.fogColor.rgb, fog), 1.0);
}
