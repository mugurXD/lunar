const uint  DITHER_SIZE   = 4;
const float DITHER_LEVELS = 16.0;
const float DITHER_CENTER = 0.5;
const float BAYER_4X4[DITHER_SIZE * DITHER_SIZE] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);

float DitherOffset(uvec2 pixel)
{
	const uvec2 cell = pixel % DITHER_SIZE;
	return (BAYER_4X4[cell.y * DITHER_SIZE + cell.x] + DITHER_CENTER) / DITHER_LEVELS;
}
