#version 450 core

//adapted from https://roystan.net/articles/outline-shader/
//to make better, read the fixing artefacts section (about viewangle)

in vec4 sunFragPos;
out vec4 color;

uniform sampler2D depthMap;
uniform sampler2D shadowMap;
uniform float lineThickness = 4;
uniform float depthThresh = 0.1;
uniform vec2 screenSize;
uniform float zNear = 0.1;
uniform float zFar = 5.0;
uniform vec3 outlineColor = vec3(0.05, 0.05, 0.05);
uniform vec3 baseColor = vec3(243.0/255.0, 223.0/255.0, 162.0/255.0);
uniform vec3 sunPos;

float linearize_depth(float d)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
} //Code from: https://stackoverflow.com/questions/51108596/linearize-depth

float SunShadow()
{
	//float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005); //depth bias to avoid shadow acne
	float bias = 0.01;
	vec3 coord = sunFragPos.xyz / sunFragPos.w;
	coord = coord * 0.5 + 0.5; //convert NDC to tex coords
	float closestDepth = texture(shadowMap, coord.xy).x; //get the depth from the shadow map
	//float closestDepth = 1.0;
	float result = coord.z + bias > closestDepth ? 0.0 : 1.0; //if the current depth is closer than closest depth then its not in shadow
	return result;
}

void main()
{
	vec2 screenPos = gl_FragCoord.xy/screenSize; //get frag screen position in range 0-1
	float minusOffset = -floor(lineThickness * 0.5); //get pixel offset for "left" of line
	float plusOffset = +floor(lineThickness * 0.5); //.. "right" ..
	vec2 uvBL = screenPos + vec2(minusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //get point half thickness to bottom left
	vec2 uvBR = screenPos + vec2(plusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //.. br
	vec2 uvTL = screenPos + vec2(minusOffset * 1.0/screenSize.x, plusOffset * 1.0/screenSize.y); //.. tl
	vec2 uvTR = screenPos + vec2(plusOffset * 1.0/screenSize.x, plusOffset* 1.0/screenSize.y); //.. tr
	float dBL = linearize_depth(texture(depthMap, uvBL).r); //sample the depth buffer in each offset and linearize it for consistent difference irrespective of depth
	float dBR = linearize_depth(texture(depthMap, uvBR).r); //..
	float dTL = linearize_depth(texture(depthMap, uvTL).r); //..
	float dTR = linearize_depth(texture(depthMap, uvTR).r); //..
	float depthDiff = length(vec2(dBL - dTR, dBR - dTL)); //get overall difference between points
	depthDiff = depthDiff > depthThresh ? 1.0 : 0.0;
	vec3 albedo = fma(outlineColor, vec3(depthDiff), baseColor * (1.0 - depthDiff));
	vec3 shadowLight = vec3(SunShadow());
	vec3 globalLight = baseColor * 0.1;
	color = vec4(albedo * (shadowLight + globalLight), 1.0);
}