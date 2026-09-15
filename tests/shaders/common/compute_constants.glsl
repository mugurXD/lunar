#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430) buffer Values
{
	uint data[];
};

layout(push_constant) uniform Constants
{
	Values source;
	Values destination;
	uint   count;
	uint   multiplier;
	uint   offset;
} constants;

layout(local_size_x = 64) in;
