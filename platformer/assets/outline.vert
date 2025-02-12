#version 450 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;

out vec4 sunFragPos;
out vec3 normal;
out vec3 position;

uniform mat4 matrix;
uniform mat4 sunMatrix;
uniform mat4 model;

void main()
{
	gl_Position = matrix * model * vec4(pos.xyz, 1.0);
	position = gl_Position.xyz;
	normal = (model * vec4(norm, 0.0)).xyz;
	sunFragPos = sunMatrix * model * vec4(pos.xyz, 1.0);
}