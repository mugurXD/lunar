#extension GL_EXT_buffer_reference : require

struct Vertex
{
	vec3  position;
	float uvX;
	vec3  normal;
	float uvY;
	vec4  color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer SceneData
{
	mat4 viewProjection;
	vec4 lightDirection;
	vec4 lightColor;
	vec4 ambientColor;
	vec4 cameraPosition;
	vec4 fogColor;
	vec4 fogRange;
};

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
