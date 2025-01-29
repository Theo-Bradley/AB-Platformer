#version 450 core

layout(location = 0) in vec3 position;

out vec2 screenPos;

uniform mat4 matrix;
uniform mat4 model;

void main()
{
	vec3 ndc = gl_Position.xyz / gl_Position.w; //perspective divide/normalize
	screenPos = ndc.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
	gl_Position = matrix * model * vec4(position.x, position.y, position.z, 1.0);
}