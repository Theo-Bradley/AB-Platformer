#version 450 core

//adapted from https://roystan.net/articles/outline-shader/
//to make better, read the fixing artefacts section (about viewangle)

out vec4 color;

uniform sampler2D depth;
uniform float lineThickness = 4;
uniform float depthThresh = 0.15;
uniform vec2 screenSize;
uniform float zNear = 0.1;
uniform float zFar = 5.0;

float linearize_depth(float d)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
} //Code from: https://stackoverflow.com/questions/51108596/linearize-depth

void main()
{
	vec2 screenPos = gl_FragCoord.xy/screenSize; //get frag screen position in range 0-1
	float minusOffset = -floor(lineThickness * 0.5); //get pixel offset for "left" of line
	float plusOffset = +floor(lineThickness * 0.5); //.. "right" ..
	vec2 uvBL = screenPos + vec2(minusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //get point half thickness to bottom left
	vec2 uvBR = screenPos + vec2(plusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //.. br
	vec2 uvTL = screenPos + vec2(minusOffset * 1.0/screenSize.x, plusOffset * 1.0/screenSize.y); //.. tl
	vec2 uvTR = screenPos + vec2(plusOffset * 1.0/screenSize.x, plusOffset* 1.0/screenSize.y); //.. tr
	float dBL = linearize_depth(texture(depth, uvBL).r); //sample the depth buffer in each offset and linearize it for consistent difference irrespective of depth
	float dBR = linearize_depth(texture(depth, uvBR).r); //..
	float dTL = linearize_depth(texture(depth, uvTL).r); //..
	float dTR = linearize_depth(texture(depth, uvTR).r); //..
	float depthDiff = length(vec2(dBL - dTR, dBR - dTL)); //get overall difference between points
	depthDiff = depthDiff > depthThresh ? 1.0 : 0.0;
	color = vec4(vec3(depthDiff), 1.0);
}