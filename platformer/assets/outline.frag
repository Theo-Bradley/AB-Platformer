#version 450 core

struct Light
{
	vec3 pos;
	vec3 color;
	float constant;
    float linear;
    float quadratic;
};

in vec4 sunFragPos;
in vec3 normal;
in vec3 position;
out vec4 color;

uniform vec3 baseColor;
uniform sampler2D normalMap;
uniform sampler2D depthMap;
uniform sampler2D shadowMap;
uniform float lineThickness = 2;
uniform float depthThresh = 0.05;
uniform float threshViewAngleMul = 16.0;
uniform float normThresh = 0.4;
uniform float zNear = 0.1;
uniform float zFar = 5.0;
uniform vec3 outlineColor = vec3(0.05, 0.05, 0.05);
uniform vec3 sunPos;
uniform vec3 camDir;
uniform vec3 camPos;
uniform vec3 sunColor = vec3(0.8, 0.8, 0.78);
#define NUM_LIGHTS 1
uniform Light lights[NUM_LIGHTS];

float linearize_depth(float d)
{
    float z_n = 2.0 * d - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
} //Code from: https://stackoverflow.com/questions/51108596/linearize-depth

float SunShadow()
{ //adapted from https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
	vec3 coord = sunFragPos.xyz / sunFragPos.w;
	coord = coord * 0.5 + 0.5; //convert NDC to tex coords
	float result = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float pcfDepth = texture(shadowMap, coord.xy + vec2(-1, -1) * texelSize).x; //sample around the actual texel
    result += coord.z > pcfDepth ? 1.0 : 0.0; //if the current depth is closer than closest depth then its not in shadow
	pcfDepth = texture(shadowMap, coord.xy + vec2(-1, 1) * texelSize).x;
    result += coord.z > pcfDepth ? 1.0 : 0.0;
	pcfDepth = texture(shadowMap, coord.xy + vec2(1, 1) * texelSize).x;
    result += coord.z > pcfDepth ? 1.0 : 0.0;
	pcfDepth = texture(shadowMap, coord.xy + vec2(1, -1) * texelSize).x;
    result += coord.z > pcfDepth ? 1.0 : 0.0;
	result /= 9.0; //convolution kernel magic
	return clamp(1.0 - result, 0.0, 1.0);
}

float SunDiffuse()
{
	vec3 sunDir = normalize(sunPos);
	return smoothstep(0.0, 0.1, dot(normal, sunDir));
}

float SunSpecular()
{
	vec3 sunDir = normalize(sunPos);
	vec3 viewDir = normalize(camPos - position);
	vec3 reflectDir = reflect(-sunDir, normal);
	float value = pow(max(dot(viewDir, reflectDir), 0.0), 256);
	return smoothstep(0.96, 0.975, value);
}

vec3 PointLightDiffuse(Light light)
{
	//phong adapted from https://learnopengl.com/Lighting/Basic-Lighting
	vec3 lightDir = light.pos - position; //get direction to light
	float dist = length(lightDir);
	lightDir = normalize(lightDir);
	//float value = smoothstep(0.0, 0.1, dot(normal, lightDir)); //this is proper toon shading, but looks funny
    float value = max(dot(normal, lightDir), 0.0); //dot normal with light dir and clamp 0-1
	float atten = 1.0 / (light.constant + light.linear * dist + 
  			     light.quadratic * (dist * dist)); //replace line 105 with fma
	return value * light.color * atten;
}

float Outline()
{
	//adapted from https://roystan.net/articles/outline-shader/
	vec2 screenSize = textureSize(normalMap, 0);
	vec2 screenPos = gl_FragCoord.xy/screenSize; //get frag screen position in range 0-1
	float minusOffset = -floor(lineThickness * 0.5); //get pixel offset for "left" of line
	float plusOffset = +floor(lineThickness * 0.5); //.. "right" ..
	//float vNorm = dot(texture(normalMap, screenPos).rgb, -camDir);
	float vNorm = (dot(normal, camDir) + 1.0) / 2; //replace with fma
	vNorm = vNorm * threshViewAngleMul + 1.0;

	vec2 uvBL = screenPos + vec2(minusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //get point half thickness to bottom left
	vec2 uvBR = screenPos + vec2(plusOffset * 1.0/screenSize.x, minusOffset * 1.0/screenSize.y); //.. br
	vec2 uvTL = screenPos + vec2(minusOffset * 1.0/screenSize.x, plusOffset * 1.0/screenSize.y); //.. tl
	vec2 uvTR = screenPos + vec2(plusOffset * 1.0/screenSize.x, plusOffset* 1.0/screenSize.y); //.. tr
	float dBL = linearize_depth(texture(depthMap, uvBL).r); //sample the depth buffer in each offset and linearize it for consistent difference irrespective of depth
	float dBR = linearize_depth(texture(depthMap, uvBR).r); //..
	float dTL = linearize_depth(texture(depthMap, uvTL).r); //..
	float dTR = linearize_depth(texture(depthMap, uvTR).r); //..
	float depthDiff = length(vec2(dBL - dTR, dBR - dTL)); //get overall difference between points
	depthDiff = depthDiff > depthThresh * vNorm ? 1.0 : 0.0;

	vec3 nBL = texture(normalMap, uvBL).rgb;
	vec3 nBR = texture(normalMap, uvBR).rgb;
	vec3 nTL = texture(normalMap, uvTL).rgb;
	vec3 nTR = texture(normalMap, uvTR).rgb;
	float normDiff = sqrt(dot(nBL - nTR, nBL - nTR) + dot(nTL - nBR, nTL - nBR));
	normDiff = normDiff > normThresh ? 1.0 : 0.0;
	
	return max(depthDiff, normDiff);
}


void main()
{
	float outline = Outline();
    vec3 diffuse = pow(baseColor, vec3(1.0/2.2)); //gamma correct the base color (colors from blender need correction)
	vec3 albedo = fma(outlineColor, vec3(outline), diffuse * (1.0 - outline));
	vec3 sunLight = fma(SunShadow(), SunDiffuse(), SunSpecular()) * sunColor; //we multiply them because they are the same light
	//SunShadow() gets the shadow from the shadow map, and SunDiffuse() calculates our shadow using sunPos
	vec3 globalLight = vec3(0.1, 0.1, 0.1);
	vec3 pointLights = vec3(0.0, 0.0, 0.0);
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		pointLights += PointLightDiffuse(lights[i]);
	}
	color = vec4(albedo * (sunLight + globalLight + pointLights), 1.0);
}