vec3 ShadeSurface(vec3 base_color, vec3 normal, vec3 world_position)
{
	const float diffuse   = max(dot(normal, -constants.scene.lightDirection.xyz), 0.0);
	const vec3  lighting  = constants.scene.ambientColor.rgb + constants.scene.lightColor.rgb * diffuse;
	const vec2  fog_range = constants.scene.fogRange.xy;
	const float distance  = length(world_position - constants.scene.cameraPosition.xyz);
	const float fog       = fog_range.y > fog_range.x ? smoothstep(fog_range.x, fog_range.y, distance) : 0.0;

	return mix(base_color * lighting, constants.scene.fogColor.rgb, fog);
}
