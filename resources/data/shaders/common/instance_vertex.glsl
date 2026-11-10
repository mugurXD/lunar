layout(location = 0)      out vec3 outNormal;
layout(location = 1) flat out vec4 outColor;
layout(location = 2)      out vec3 outWorldPosition;
layout(location = 3) flat out vec2 outFade;

const vec4 CULLED_POSITION = vec4(0.0, 0.0, 2.0, 1.0);

float FadeProgress(vec2 band, float distance)
{
	return band.y > band.x ? smoothstep(band.x, band.y, distance) : step(band.x, distance);
}

bool IsFadedOut(vec2 fade)
{
	return fade.x <= 0.0 || fade.y >= 1.0;
}

void EmitInstanceVertex(vec3 world_position, vec3 world_origin, vec3 world_normal, vec4 color)
{
	const float distance = length(world_origin - constants.scene.cameraPosition.xyz);

	outNormal        = world_normal;
	outColor         = color;
	outWorldPosition = world_position;
	outFade          = vec2(FadeProgress(constants.fadeBands.xy, distance), FadeProgress(constants.fadeBands.zw, distance));
	gl_Position      = IsFadedOut(outFade) ? CULLED_POSITION : constants.scene.viewProjection * vec4(world_position, 1.0);
}
