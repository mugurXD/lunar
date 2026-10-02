#version 460
#include "common/mesh_interface.glsl"

const float HALF = 0.5;

layout(location = 0) out vec4  outColor;
layout(location = 1) out float outHeight;

void main()
{
	const Vertex vertex = constants.vertexBuffer.vertices[gl_VertexIndex];

	outColor    = vertex.color;
	outHeight   = vertex.position.y * HALF + HALF;
	gl_Position = constants.scene.viewProjection * constants.draw.model * vec4(vertex.position, 1.0);
}
