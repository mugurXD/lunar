#extension GL_EXT_nonuniform_qualifier : require

const uint SAMPLER_LINEAR_CLAMP       = 0;
const uint SAMPLER_LINEAR_REPEAT      = 1;
const uint SAMPLER_NEAREST_CLAMP      = 2;
const uint SAMPLER_NEAREST_REPEAT     = 3;
const uint SAMPLER_ANISOTROPIC_REPEAT = 4;
const uint SAMPLER_COUNT              = 5;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 1) uniform sampler   samplers[SAMPLER_COUNT];

vec4 SampleTexture(uint texture_index, uint sampler_index, vec2 uv)
{
	return texture(sampler2D(textures[nonuniformEXT(texture_index)], samplers[sampler_index]), uv);
}

vec4 FetchTexel(uint texture_index, ivec2 texel)
{
	return texelFetch(sampler2D(textures[nonuniformEXT(texture_index)], samplers[SAMPLER_NEAREST_CLAMP]), texel, 0);
}
