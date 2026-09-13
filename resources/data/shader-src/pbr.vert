#version 450 core
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_color;
layout (location = 3) in float in_uv_x;
layout (location = 4) in float in_uv_y;

layout (std140, binding = 0) uniform SceneData
{
	mat4 projection;
	mat4 view;
	vec3 camera_pos;
};

layout (std140, binding = 1) uniform MeshData
{
	mat4 model;
};

struct Material
{
	vec2 atlasBegin;
	vec2 atlasEnd;
	float metallic;
	float roughness;
	float ao;
};

layout (std430, binding = 2) buffer MaterialBuffer
{
	Material materials[20];
	int primitiveToMaterial[];
};

//layout (std140, binding = 1) uniform LightingData
//{
//	float metallic;
//	float roughness;
//	float ao;
//	int lights_count;
//	vec3 light_pos[10];
//	vec3 light_col[10];
//    int is_hdr_cubemap;
//};

layout (location = 0) out vec2 out_uv;
layout (location = 1) out vec3 out_world;
layout (location = 2) out vec3 out_normal;

void main()
{
	out_uv      = vec2(in_uv_x, in_uv_y);
	out_world   = vec3(model * vec4(in_pos, 1.0));
	out_normal  = in_normal;
	//out_normal  = transpose(inverse(mat3(model))) * in_normal;
	gl_Position = projection * view * model * vec4(in_pos.xyz, 1);
}
