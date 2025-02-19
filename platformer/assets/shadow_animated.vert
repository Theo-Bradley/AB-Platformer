#version 450 core

layout(location = 0) in vec3 position;
layout(location = 2) in vec3 nextPosition;

uniform mat4 sunMatrix;
uniform mat4 model;
uniform float animFac;

void main()
{
	gl_Position = sunMatrix * model * vec4(mix(position, nextPosition, animFac), 1.0);
}