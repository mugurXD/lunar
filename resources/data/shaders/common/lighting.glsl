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

// Polynomial fit from Jimenez et al. 2016 for light that bounces between occluders, which lightens occlusion on brighter surfaces.
const vec2 MULTI_BOUNCE_A = vec2(2.0404, -0.3324);
const vec2 MULTI_BOUNCE_B = vec2(-4.7951, 0.6417);
const vec2 MULTI_BOUNCE_C = vec2(2.7552, 0.6903);

float ShadowTap(uint shadow_map, vec2 texel_position, vec2 map_size, float depth)
{
	const vec2 base = floor(texel_position);
	const vec4 lit  = step(GatherTexels(shadow_map, (base + 1.0) / map_size), vec4(depth));

	return MixGathered(lit, texel_position - base);
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

// Occlusion can be rendered below screen resolution, and linear filtering of 32 bit float textures is optional in Vulkan, so it is upsampled from gathered texels.
float AmbientVisibility()
{
	const uint occlusion = constants.scene.ambientOcclusion;
	if (occlusion == INVALID_TEXTURE_INDEX)
		return FULLY_LIT;

	const vec2 map_size       = vec2(TextureSize(occlusion));
	const vec2 texel_position = gl_FragCoord.xy * constants.scene.screenTexelSize * map_size - HALF;
	const vec2 base           = floor(texel_position);

	return MixGathered(GatherTexels(occlusion, (base + 1.0) / map_size), texel_position - base);
}

vec3 MultiBounce(float visibility, vec3 albedo)
{
	const vec3 a = albedo * MULTI_BOUNCE_A.x + MULTI_BOUNCE_A.y;
	const vec3 b = albedo * MULTI_BOUNCE_B.x + MULTI_BOUNCE_B.y;
	const vec3 c = albedo * MULTI_BOUNCE_C.x + MULTI_BOUNCE_C.y;

	return max(vec3(visibility), ((visibility * a + b) * visibility + c) * visibility);
}

vec3 ShadeSurface(vec3 base_color, vec3 normal, vec3 world_position)
{
	const float diffuse    = max(dot(normal, -constants.scene.lightDirection.xyz), 0.0);
	const float sunlight   = diffuse > 0.0 ? diffuse * SunShadow(world_position, normal) : 0.0;
	const float sky_weight = normal.y * HALF + HALF;
	const vec3  sky_light  = mix(constants.scene.groundColor.rgb, constants.scene.skyColor.rgb, sky_weight);
	const vec3  ambient    = sky_light * MultiBounce(AmbientVisibility(), base_color);
	const vec3  lighting   = ambient + constants.scene.lightColor.rgb * sunlight;
	const vec2  fog_range  = constants.scene.fogRange.xy;
	const float distance   = length(world_position - constants.scene.cameraPosition.xyz);
	const float fog        = fog_range.y > fog_range.x ? smoothstep(fog_range.x, fog_range.y, distance) : 0.0;

	return mix(base_color * lighting, constants.scene.fogColor.rgb, fog);
}
