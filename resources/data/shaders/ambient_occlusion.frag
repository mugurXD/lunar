#version 460
#include "common/ambient_occlusion.glsl"
#include "common/dither.glsl"

const uint  SLICE_COUNT         = 4;
const uint  STEP_COUNT          = 4;
const float PI                  = 3.14159265358979;
const float HALF_PI             = PI * HALF;
const float FALLOFF_FRACTION    = 0.6;
const float SAMPLE_DISTRIBUTION = 2.0;
const float MIN_SAMPLE_PIXELS   = 1.3;
const float FADE_START_PIXELS   = 2.0;
const float FADE_END_PIXELS     = 8.0;
const float MAX_SCREEN_RADIUS   = 0.15;
const float MIN_VISIBILITY      = 0.03;
const float MIN_SAMPLE_DISTANCE = 1e-6;

layout(location = 0) out float outVisibility;

struct Surface
{
	vec2 pixel;
	vec3 position;
	vec3 viewVector;
	vec3 normal;
};

struct Sampling
{
	float screenRadius;
	float minStep;
	float stepNoise;
	float falloffRange;
};

float HorizonCos(Surface surface, Sampling sampling, vec2 direction, float low_horizon_cos)
{
	float horizon_cos = low_horizon_cos;

	for (uint step_index = 0; step_index < STEP_COUNT; step_index++)
	{
		// Squaring the step spacing packs samples near the center, where occluders matter most.
		const float progress = pow((float(step_index) + sampling.stepNoise) / float(STEP_COUNT), SAMPLE_DISTRIBUTION) + sampling.minStep;
		const vec3  delta    = ViewPosition(surface.pixel + round(direction * progress * sampling.screenRadius)) - surface.position;
		const float distance = max(length(delta), MIN_SAMPLE_DISTANCE);
		const float weight   = clamp((constants.radius - distance) / sampling.falloffRange, 0.0, 1.0);

		horizon_cos = max(horizon_cos, mix(low_horizon_cos, dot(delta, surface.viewVector) / distance, weight));
	}

	return clamp(horizon_cos, -1.0, 1.0);
}

float IntegrateArc(float horizon, float normal_angle, float cos_normal)
{
	return (cos_normal + 2.0 * horizon * sin(normal_angle) - cos(2.0 * horizon - normal_angle)) * 0.25;
}

float SliceVisibility(Surface surface, Sampling sampling, float angle)
{
	// Screen rows grow downwards while view space Y points up.
	const vec2  direction        = vec2(cos(angle), sin(angle));
	const vec3  view_direction   = vec3(direction.x, -direction.y, 0.0);
	const vec3  ortho_direction  = view_direction - dot(view_direction, surface.viewVector) * surface.viewVector;
	const vec3  axis             = normalize(cross(ortho_direction, surface.viewVector));
	const vec3  projected_normal = surface.normal - axis * dot(surface.normal, axis);
	const float projected_length = length(projected_normal);
	const float cos_normal       = clamp(dot(projected_normal, surface.viewVector) / projected_length, 0.0, 1.0);
	const float normal_angle     = sign(dot(ortho_direction, projected_normal)) * acos(cos_normal);

	// The tangent plane is the lowest horizon either side can have, so samples fading out of the radius fall back to it.
	const float forward_cos      = HorizonCos(surface, sampling, direction, cos(normal_angle + HALF_PI));
	const float backward_cos     = HorizonCos(surface, sampling, -direction, cos(normal_angle - HALF_PI));
	const float forward_horizon  = normal_angle + clamp(acos(forward_cos) - normal_angle, -HALF_PI, HALF_PI);
	const float backward_horizon = normal_angle + clamp(-acos(backward_cos) - normal_angle, -HALF_PI, HALF_PI);

	return projected_length * (IntegrateArc(forward_horizon, normal_angle, cos_normal) + IntegrateArc(backward_horizon, normal_angle, cos_normal));
}

void main()
{
	const vec2  pixel         = gl_FragCoord.xy;
	const vec3  position      = ViewPosition(pixel);
	const float pixel_size    = position.z * NDC_SPAN * constants.tanHalfFov.y * constants.texelSize.y;
	const float radius_pixels = constants.radius / pixel_size;

	// Far away the radius shrinks below a pixel and there is nothing left to resolve, which also covers the sky.
	// The depth prepass stops drawing at the end of the fade range, so occlusion has to be gone by then.
	const float pixel_fade    = smoothstep(FADE_START_PIXELS, FADE_END_PIXELS, radius_pixels);
	const float distance_fade = 1.0 - smoothstep(constants.fadeRange.x, constants.fadeRange.y, length(position));
	const float fade          = pixel_fade * distance_fade;
	if (fade <= 0.0)
	{
		outVisibility = FULLY_VISIBLE;
		return;
	}

	// Close to the camera the radius covers most of the screen, so it is capped to keep samples near the center cache friendly.
	const float    screen_radius = min(radius_pixels, MAX_SCREEN_RADIUS / constants.texelSize.y);
	const Surface  surface       = Surface(pixel, position, normalize(-position), ViewNormal(pixel, position));
	const Sampling sampling      = Sampling(screen_radius, MIN_SAMPLE_PIXELS / screen_radius, DitherOffset(uvec2(pixel).yx), constants.radius * FALLOFF_FRACTION);
	const float    slice_offset  = DitherOffset(uvec2(pixel));
	float          visibility    = 0.0;

	for (uint slice = 0; slice < SLICE_COUNT; slice++)
		visibility += SliceVisibility(surface, sampling, (float(slice) + slice_offset) * PI / float(SLICE_COUNT));

	const float occluded = pow(clamp(visibility / float(SLICE_COUNT), MIN_VISIBILITY, FULLY_VISIBLE), constants.intensity);
	outVisibility = mix(FULLY_VISIBLE, occluded, fade);
}
