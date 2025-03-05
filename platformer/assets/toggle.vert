#version 450 core

layout(location = 0) in vec3 position;

out vec2 uv;
uniform vec2 scale;
uniform vec2 offset;

void main()
{
	gl_Position = vec4(position.x + offset.x * 2.0 * scale.x, position.z + offset.y * 2.0 * scale.y, 0.0, 1.0);
	uv = vec2(position.x - offset.x, 0.0 - position.z + offset.y) + vec2(1.0, 1.0);
}