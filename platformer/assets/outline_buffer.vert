#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 norm;

out vec3 screenSpaceNormal;

uniform mat4 matrix;
uniform mat4 model;
void main()
{
	gl_Position = matrix * model * vec4(position, 1.0);
	screenSpaceNormal = (matrix * model * vec4(norm, 0.0)).xyz;
}