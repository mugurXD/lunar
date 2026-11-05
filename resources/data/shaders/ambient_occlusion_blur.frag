#version 460
#include "common/ambient_occlusion.glsl"
#include "common/dither.glsl"

// The kernel covers exactly one dither tile, so every pixel averages each slice rotation and step offset once.
const int   BLUR_FIRST_OFFSET = -int(DITHER_SIZE / 2);
const int   BLUR_LAST_OFFSET  = BLUR_FIRST_OFFSET + int(DITHER_SIZE) - 1;
const float PLANE_TOLERANCE   = 0.02;

layout(location = 0) out float outVisibility;

// Weighting by distance to the center's plane instead of by depth keeps blurring across surfaces seen at grazing angles, like the road ahead.
void main()
{
	const vec2  pixel     = gl_FragCoord.xy;
	const vec3  position  = ViewPosition(pixel);
	const vec3  normal    = ViewNormal(pixel, position);
	const float tolerance = position.z * PLANE_TOLERANCE;
	float       total     = 0.0;
	float       weights   = 0.0;

	for (int y = BLUR_FIRST_OFFSET; y <= BLUR_LAST_OFFSET; y++)
		for (int x = BLUR_FIRST_OFFSET; x <= BLUR_LAST_OFFSET; x++)
		{
			const vec2  tap    = pixel + vec2(x, y);
			const float weight = max(1.0 - abs(dot(ViewPosition(tap) - position, normal)) / tolerance, 0.0);

			total   += weight * SampleTexture(constants.occlusionTexture, SAMPLER_NEAREST_CLAMP, tap * constants.texelSize).r;
			weights += weight;
		}

	outVisibility = total / weights;
}
