#include "mesh_types.glsl"
#include "instance_types.glsl"

layout(push_constant) uniform InstanceConstants
{
	SceneData      scene;
	InstanceBuffer instanceBuffer;
	VertexBuffer   vertexBuffer;
	mat4           transform;
	vec4           fadeBands;
} constants;
