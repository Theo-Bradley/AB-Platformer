#version 450 core

in vec3 screenSpaceNormal;
out vec4 color;

void main()
{
	color = vec4(screenSpaceNormal, 1.0);
}