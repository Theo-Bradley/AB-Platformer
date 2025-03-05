#version 450 core

in vec2 uv;
out vec4 color;

uniform sampler2D tex;
uniform vec3 colour;

void main()
{
	float val = texture(tex, uv).r;
	if (val < 0.95)
	{
		discard;
	}
	color = vec4(colour, 1.0);
}