#version 460
#include "common/mesh_interface.glsl"

layout(location = 0)      out vec3 outNormal;
layout(location = 1) flat out vec4 outColor;
layout(location = 2)      out vec3 outWorldPosition;
layout(location = 3) flat out vec3 outFacetNormal;

void main()
{
	const Vertex vertex         = constants.vertexBuffer.vertices[gl_VertexIndex];
	const vec4   world_position = constants.draw.model * vec4(vertex.position, 1.0);

	outNormal        = mat3(constants.draw.normalMatrix) * vertex.normal;
	outColor         = vertex.color;
	outWorldPosition = world_position.xyz;
	outFacetNormal   = outNormal;
	gl_Position      = constants.scene.viewProjection * world_position;
}
