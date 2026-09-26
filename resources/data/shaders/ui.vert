#version 460
#include "common/ui_interface.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUv;

void main()
{
	const UiVertex vertex = constants.vertexBuffer.vertices[gl_VertexIndex];

	outColor    = unpackUnorm4x8(vertex.color);
	outUv       = vertex.uv;
	gl_Position = constants.transform * vec4(vertex.position + constants.translation, 0.0, 1.0);
}
