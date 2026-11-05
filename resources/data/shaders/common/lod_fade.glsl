#include "dither.glsl"

const uint  MASK_SAMPLES          = 32;
const float RADICAL_INVERSE_SCALE = 1.0 / 4294967296.0;

// Both levels of detail test the same per sample thresholds, so each sample ends up covered by exactly one of them.
// Bit reversed sample indices spread any power of two of samples evenly, which keeps the split right without knowing the sample count.
void ApplyLodFade(vec2 fade)
{
	const float offset = DitherOffset(uvec2(gl_FragCoord.xy));
	int         mask   = 0;

	for (uint sample_index = 0; sample_index < MASK_SAMPLES; sample_index++)
	{
		const float threshold = fract(offset + float(bitfieldReverse(sample_index)) * RADICAL_INVERSE_SCALE);
		if (threshold >= fade.y && threshold < fade.x)
			mask |= int(1u << sample_index);
	}

	gl_SampleMask[0] = mask;
}
