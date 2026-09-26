#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct UiVertex
{
	vec2 position;
	uint color;
	vec2 uv;
};

layout(buffer_reference, scalar) readonly buffer UiVertexBuffer
{
	UiVertex vertices[];
};

layout(push_constant) uniform UiConstants
{
	mat4           transform;
	vec2           translation;
	UiVertexBuffer vertexBuffer;
	uint           texture;
	uint           linearOutput;
} constants;
