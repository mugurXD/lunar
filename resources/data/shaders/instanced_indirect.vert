#version 460
#include "common/mesh_types.glsl"
#include "common/instance_types.glsl"

layout(push_constant) uniform VisibleInstanceConstants
{
	SceneData      scene;
	InstanceBuffer instanceBuffer;
	VertexBuffer   vertexBuffer;
	vec4           fadeBands;
} constants;

#include "common/instance_vertex.glsl"

void main()
{
	const Vertex   vertex   = constants.vertexBuffer.vertices[gl_VertexIndex];
	const Instance instance = constants.instanceBuffer.instances[gl_InstanceIndex];

	EmitInstanceVertex(Rotate(instance.rotation, vertex.position * instance.scale) + instance.position,
	                   instance.position,
	                   Rotate(instance.rotation, vertex.normal),
	                   vertex.color);
}
