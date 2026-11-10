#version 460
#include "common/instance_interface.glsl"
#include "common/instance_vertex.glsl"

void main()
{
	const Vertex   vertex         = constants.vertexBuffer.vertices[gl_VertexIndex];
	const Instance instance       = constants.instanceBuffer.instances[gl_InstanceIndex];
	const vec3     local_position = Rotate(instance.rotation, vertex.position * instance.scale) + instance.position;

	EmitInstanceVertex((constants.transform * vec4(local_position, 1.0)).xyz,
	                   (constants.transform * vec4(instance.position, 1.0)).xyz,
	                   mat3(constants.transform) * Rotate(instance.rotation, vertex.normal),
	                   vertex.color);
}
