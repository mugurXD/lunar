#include "textures.glsl"

const float HALF              = 0.5;
const float NDC_SPAN          = 2.0;
const float FULLY_VISIBLE     = 1.0;
const float MIN_STORED_DEPTH  = 1e-7;
const vec2  UV_TO_CLIP_SCALE  = vec2(NDC_SPAN, -NDC_SPAN);
const vec2  NEXT_COLUMN       = vec2(1.0, 0.0);
const vec2  NEXT_ROW          = vec2(0.0, 1.0);
const vec3  MISSING_NEIGHBOR  = vec3(0.0);

layout(push_constant) uniform AmbientOcclusionConstants
{
	vec2  tanHalfFov;
	vec2  texelSize;
	vec2  fadeRange;
	float nearPlane;
	float radius;
	float intensity;
	uint  depthTexture;
	uint  occlusionTexture;
} constants;

// Depth is reverse Z with an infinite far plane, so the sky stores zero and is clamped to a very distant but finite point.
vec3 ViewPosition(vec2 pixel)
{
	const vec2  uv       = pixel * constants.texelSize;
	const float depth    = SampleTexture(constants.depthTexture, SAMPLER_NEAREST_CLAMP, uv).r;
	const float distance = constants.nearPlane / max(depth, MIN_STORED_DEPTH);

	return vec3((uv - HALF) * UV_TO_CLIP_SCALE * constants.tanHalfFov * distance, distance);
}

// Neighbors past the screen edge clamp back onto the center texel, which leaves a zero delta that cannot span a plane.
vec3 FlatterDelta(vec3 forward, vec3 backward)
{
	if (forward == MISSING_NEIGHBOR)
		return backward;

	if (backward == MISSING_NEIGHBOR)
		return forward;

	return abs(forward.z) < abs(backward.z) ? forward : backward;
}

// Taking the flatter neighbor on each axis keeps the normal on the center's own face when the pixel sits on a depth edge.
vec3 ViewNormal(vec2 pixel, vec3 center)
{
	const vec3 horizontal = FlatterDelta(ViewPosition(pixel + NEXT_COLUMN) - center, center - ViewPosition(pixel - NEXT_COLUMN));
	const vec3 vertical   = FlatterDelta(ViewPosition(pixel + NEXT_ROW) - center, center - ViewPosition(pixel - NEXT_ROW));
	const vec3 normal     = normalize(cross(horizontal, vertical));

	return dot(normal, center) > 0.0 ? -normal : normal;
}
