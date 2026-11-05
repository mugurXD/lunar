const uint  MASK_SAMPLES          = 32;
const uint  DITHER_SIZE           = 4;
const float DITHER_LEVELS         = 16.0;
const float DITHER_CENTER         = 0.5;
const float RADICAL_INVERSE_SCALE = 1.0 / 4294967296.0;
const float BAYER_4X4[DITHER_SIZE * DITHER_SIZE] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

// Both levels of detail test the same per sample thresholds, so each sample ends up covered by exactly one of them.
// Bit reversed sample indices spread any power of two of samples evenly, which keeps the split right without knowing the sample count.
void ApplyLodFade(vec2 fade)
{
	const uvec2 cell   = uvec2(gl_FragCoord.xy) % DITHER_SIZE;
	const float offset = (BAYER_4X4[cell.y * DITHER_SIZE + cell.x] + DITHER_CENTER) / DITHER_LEVELS;
	int         mask   = 0;

	for (uint sample_index = 0; sample_index < MASK_SAMPLES; sample_index++)
	{
		const float threshold = fract(offset + float(bitfieldReverse(sample_index)) * RADICAL_INVERSE_SCALE);
		if (threshold >= fade.y && threshold < fade.x)
			mask |= int(1u << sample_index);
	}

	gl_SampleMask[0] = mask;
}
