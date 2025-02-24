#version 450 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec3 nextPos;
layout(location = 3) in vec3 nextNorm;

out vec4 sunFragPos;
out vec3 normal;
out vec3 position;

uniform mat4 matrix;
uniform mat4 sunMatrix;
uniform mat4 model;
uniform float animFac;

void main()
{
	gl_Position = matrix * model * vec4(mix(pos, nextPos, animFac), 1.0);
	position = (model * vec4(pos.xyz, 1.0)).xyz;
	normal = normalize((model * vec4(mix(norm, nextNorm, animFac), 0.0)).xyz);
	sunFragPos = sunMatrix * model * vec4(pos.xyz, 1.0);
}