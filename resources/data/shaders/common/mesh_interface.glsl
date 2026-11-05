#include "mesh_types.glsl"

struct DrawData
{
	mat4 model;
	mat4 normalMatrix;
};

layout(buffer_reference, std430) readonly buffer DrawBuffer
{
	DrawData draws[];
};

layout(push_constant) uniform MeshConstants
{
	SceneData    scene;
	DrawBuffer   drawBuffer;
	VertexBuffer vertexBuffer;
} constants;
