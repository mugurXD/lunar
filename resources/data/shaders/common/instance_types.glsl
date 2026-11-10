#extension GL_EXT_buffer_reference : require

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

vec3 Rotate(vec4 rotation, vec3 vector)
{
	const vec3 doubled_cross = 2.0 * cross(rotation.xyz, vector);
	return vector + rotation.w * doubled_cross + cross(rotation.xyz, doubled_cross);
}
