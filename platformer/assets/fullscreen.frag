#version 450 core

in vec2 uv;
out vec4 color;

uniform sampler2D mainMenuTex;

void main()
{
	color = texture(mainMenuTex, uv);
}