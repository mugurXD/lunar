#version 460
#include "common/instance_interface.glsl"

layout(location = 0)      out vec3 outNormal;
layout(location = 1) flat out vec4 outColor;
layout(location = 2)      out vec3 outWorldPosition;
layout(location = 3) flat out vec2 outFade;

const vec4 CULLED_POSITION = vec4(0.0, 0.0, 2.0, 1.0);

float FadeProgress(vec2 band, float distance)
{
	return band.y > band.x ? smoothstep(band.x, band.y, distance) : step(band.x, distance);
}

bool IsFadedOut(vec2 fade)
{
	return fade.x <= 0.0 || fade.y >= 1.0;
}

void main()
{
	const Vertex   vertex         = constants.vertexBuffer.vertices[gl_VertexIndex];
	const Instance instance       = constants.instanceBuffer.instances[gl_InstanceIndex];
	const vec3     local_position = Rotate(instance.rotation, vertex.position * instance.scale) + instance.position;
	const vec4     world_position = constants.transform * vec4(local_position, 1.0);
	const vec3     world_origin   = (constants.transform * vec4(instance.position, 1.0)).xyz;
	const float    distance       = length(world_origin - constants.scene.cameraPosition.xyz);

	outNormal        = mat3(constants.transform) * Rotate(instance.rotation, vertex.normal);
	outColor         = vertex.color;
	outWorldPosition = world_position.xyz;
	outFade          = vec2(FadeProgress(constants.fadeBands.xy, distance), FadeProgress(constants.fadeBands.zw, distance));
	gl_Position      = IsFadedOut(outFade) ? CULLED_POSITION : constants.scene.viewProjection * world_position;
}
