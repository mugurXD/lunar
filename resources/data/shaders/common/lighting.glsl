#include "textures.glsl"

const float HALF                 = 0.5;
const float FULLY_LIT            = 1.0;
const float SHADOW_NORMAL_OFFSET = 1.5;
const float SHADOW_LIGHT_OFFSET  = 1.0;
const float SHADOW_EDGE_TEXELS   = 3.0;
const int   SHADOW_FILTER_RADIUS = 1;
const float SHADOW_FILTER_TAPS   = float((2 * SHADOW_FILTER_RADIUS + 1) * (2 * SHADOW_FILTER_RADIUS + 1));

// Viewports are flipped, so positive clip space Y lands in the top rows of a shadow map.
const vec2 CLIP_TO_UV_SCALE = vec2(HALF, -HALF);

float ShadowTap(uint shadow_map, vec2 texel_position, vec2 map_size, float depth)
{
	const vec2 base    = floor(texel_position);
	const vec2 weights = texel_position - base;
	const vec4 lit     = step(GatherTexels(shadow_map, (base + 1.0) / map_size), vec4(depth));

	return mix(mix(lit.w, lit.z, weights.x), mix(lit.x, lit.y, weights.x), weights.y);
}

float FilterShadow(uint shadow_map, vec2 uv, float depth)
{
	const vec2 map_size       = vec2(TextureSize(shadow_map));
	const vec2 texel_position = uv * map_size - HALF;
	float      lit            = 0.0;

	for (int y = -SHADOW_FILTER_RADIUS; y <= SHADOW_FILTER_RADIUS; y++)
		for (int x = -SHADOW_FILTER_RADIUS; x <= SHADOW_FILTER_RADIUS; x++)
			lit += ShadowTap(shadow_map, texel_position + vec2(x, y), map_size, depth);

	return lit / SHADOW_FILTER_TAPS;
}

float SunShadow(vec3 world_position, vec3 normal)
{
	const vec2  shadow_range = constants.scene.shadowRange.xy;
	const float distance     = length(world_position - constants.scene.cameraPosition.xyz);
	if (distance >= shadow_range.y)
		return FULLY_LIT;

	for (uint index = 0; index < SHADOW_CASCADE_COUNT; index++)
	{
		const ShadowCascade cascade = constants.scene.shadowCascades[index];
		const vec3          offset  = normal * SHADOW_NORMAL_OFFSET - constants.scene.lightDirection.xyz * SHADOW_LIGHT_OFFSET;
		const vec4          clip    = cascade.viewProjection * vec4(world_position + offset * cascade.texelSize, 1.0);
		const vec2          uv      = clip.xy * CLIP_TO_UV_SCALE + HALF;
		const vec2          margin  = SHADOW_EDGE_TEXELS / vec2(TextureSize(cascade.shadowMap));

		if (clip.z > 0.0 && all(greaterThan(uv, margin)) && all(lessThan(uv, 1.0 - margin)))
			return mix(FilterShadow(cascade.shadowMap, uv, clip.z), FULLY_LIT, smoothstep(shadow_range.x, shadow_range.y, distance));
	}

	return FULLY_LIT;
}

vec3 FaceNormal(vec3 world_position, vec3 interpolated_normal)
{
	const vec3 face_normal = normalize(cross(dFdx(world_position), dFdy(world_position)));
	return dot(face_normal, interpolated_normal) < 0.0 ? -face_normal : face_normal;
}

vec3 ShadeSurface(vec3 base_color, vec3 normal, vec3 world_position)
{
	const float diffuse    = max(dot(normal, -constants.scene.lightDirection.xyz), 0.0);
	const float sunlight   = diffuse > 0.0 ? diffuse * SunShadow(world_position, normal) : 0.0;
	const float sky_weight = normal.y * HALF + HALF;
	const vec3  ambient    = mix(constants.scene.groundColor.rgb, constants.scene.skyColor.rgb, sky_weight);
	const vec3  lighting   = ambient + constants.scene.lightColor.rgb * sunlight;
	const vec2  fog_range  = constants.scene.fogRange.xy;
	const float distance   = length(world_position - constants.scene.cameraPosition.xyz);
	const float fog        = fog_range.y > fog_range.x ? smoothstep(fog_range.x, fog_range.y, distance) : 0.0;

	return mix(base_color * lighting, constants.scene.fogColor.rgb, fog);
}
