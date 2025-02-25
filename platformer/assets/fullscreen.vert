#version 450 core

layout(location = 0) in vec3 position;

out vec2 uv;

void main()
{
	gl_Position = vec4(position.x * 2.0, position.z * 2.0, 0.0, 1.0);
	uv = (vec2(gl_Position.x, 0.0 - gl_Position.y) + vec2(1.0, 1.0)) / 2.0;
}