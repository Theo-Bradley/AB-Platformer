#version 450 core

layout(location = 0) in vec3 position;

uniform mat4 matrix;

void main()
{
	gl_Position = matrix * vec4(position.x, position.y, position.z, 1.0);
}