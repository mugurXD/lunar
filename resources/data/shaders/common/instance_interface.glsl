#include "mesh_types.glsl"

struct Instance
{
	vec3  position;
	float scale;
	vec4  rotation;
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer
{
	Instance instances[];
};

layout(push_constant) uniform InstanceConstants
{
	SceneData      scene;
	InstanceBuffer instanceBuffer;
	VertexBuffer   vertexBuffer;
	mat4           transform;
} constants;
