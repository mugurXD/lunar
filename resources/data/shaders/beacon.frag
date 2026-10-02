#version 460

layout(location = 0) in vec4  inColor;
layout(location = 1) in float inHeight;

layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(inColor.rgb, inColor.a * (1.0 - inHeight));
}
