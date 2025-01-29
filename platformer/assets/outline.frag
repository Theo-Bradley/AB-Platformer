#version 450 core

in vec2 screenPos;

out vec4 color;

uniform sampler2D depth;
uniform float lineThickness = 4;
uniform vec2 screenSize;
uniform float zNear = 0.01;
uniform float zFar = 10.0;

float linearize_depth(float d)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
} //Code from: https://stackoverflow.com/questions/51108596/linearize-depth

void main()
{
	float minusOffset = -floor(lineThickness * 0.5); //get pixel offset for "left" of line
	float plusOffset = +floor(lineThickness * 0.5); //.. "right" ..
	vec2 uvBL = screenPos + vec2(minusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y);
	vec2 uvBR = screenPos + vec2(plusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y);
	vec2 uvTL = screenPos + vec2(minusOffset * 1.0/screenSize.x, plusOffset * 1.0/screenSize.y);
	vec2 uvTR = screenPos + vec2(plusOffset* 1.0/screenSize.x, plusOffset* 1.0/screenSize.y);
	float dBL = linearize_depth(texture(depth, uvBL).r);
	float dBR = linearize_depth(texture(depth, uvBR).r);
	float dTL = linearize_depth(texture(depth, uvTL).r);
	float dTR = linearize_depth(texture(depth, uvTR).r);
	float depthDiff = length(vec2(dBL - dBR, dTL - dTR));
	color = vec4(vec3(smoothstep(0.0, 1.0, depthDiff)), 1.0);
	//color = vec4(243.0/255.0, 223.0/255.0, 162.0/255.0, 1.0);
}