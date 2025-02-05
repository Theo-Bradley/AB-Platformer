#version 450 core

layout(location = 0) in vec3 position;

out vec4 sunFragPos;

uniform mat4 matrix;
uniform mat4 sunMatrix;
uniform mat4 model;

void main()
{
	gl_Position = matrix * model * vec4(position.xyz, 1.0);
	sunFragPos = sunMatrix * model * vec4(position.xyz, 1.0);
}