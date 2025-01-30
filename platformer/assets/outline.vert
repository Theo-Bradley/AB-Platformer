#version 450 core

layout(location = 0) in vec3 position;

uniform mat4 matrix;
uniform mat4 model;

void main()
{
	gl_Position = matrix * model * vec4(position.x, position.y, position.z, 1.0);
	//vec3 ndc = gl_Position.xyz / gl_Position.w; //perspective divide/normalize
	//screenPos = ndc.xy * 0.5 + 0.5; //ndc is -1 to 1 in GL. scale for 0 to 1
}