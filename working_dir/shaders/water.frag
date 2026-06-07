#version 430
/*
	File Name	: color.vert
	Author		: Bora Yalciner
	Description	:

		Basic fragment shader that just outputs
		color to the FBO
*/

// Definitions
// These locations must match between vertex/fragment shaders
#define IN_UV		 layout(location = 0)
#define IN_NORMAL	 layout(location = 1)
#define IN_WORLD_POS layout(location = 2)

// This output must match to the COLOR_ATTACHMENTi (where 'i' is this location)
#define OUT_FBO		layout(location = 0)

// This must match GL_TEXTUREi (where 'i' is this binding)
#define T_HDR	layout(binding = 0)

// This must match the first parameter of glUniform...() calls
#define U_SUN_DIR	  layout(location = 0)
#define U_SUN_POW	  layout(location = 1)
#define U_WATER_COLOR layout(location = 2)
#define U_CAM_POS     layout(location = 3)
#define U_WATER_IOR   layout(location = 4)

// Input
in IN_UV		vec2 fUV;
in IN_NORMAL    vec3 fNormal;
in IN_WORLD_POS vec3 fWorldPos;

// Output
// This parameter goes to the framebuffer
out OUT_FBO vec4 fboColor;

// Uniforms
U_SUN_DIR     uniform vec3 uLDir;
U_SUN_POW     uniform float uLPow;
U_WATER_COLOR uniform vec3 uWaterColor;
U_CAM_POS     uniform vec3 uCamPos;
U_WATER_IOR	  uniform float uWaterIOR;

// Textures
uniform T_HDR sampler2D tHDR;

const float cAirIOR = 1;

#define PI 3.14159265
#define INV_PI 0.3183099

vec2 DirToSphericalUV(vec3 v)
{
	float azimuth = atan(v.x, v.z);
	float incl = acos(clamp(v.y, -1, 1));
	vec2 uv = vec2((azimuth + PI) * 0.5 * INV_PI,
					  1.0f - incl * INV_PI);
	return uv;
}

void main(void)
{
	vec3 N = fNormal;
	vec3 V = normalize(uCamPos - fWorldPos);
	float etaRatio = (cAirIOR - uWaterIOR) / (cAirIOR + uWaterIOR);
	etaRatio = etaRatio * etaRatio;

	//
	float cosTheta = clamp(dot(N, V), 0, 1);
	float f = 1 - cosTheta;
	float f2 =  f * f;
	float f4 = f2 * f2;
	float f5 = f4 * f;

	// Percentage of reflectance;
	float fresnelTerm = etaRatio + (1 - etaRatio) * f5;

	vec3 R = normalize(reflect(-V, N));
	vec2 rUV = DirToSphericalUV(R);
	vec3 hdrLum = textureLod(tHDR, rUV, 0).rgb;
	vec3 result = uWaterColor * hdrLum * fresnelTerm;

	// TODO:
	//vec3 result = uWaterColor;

	// Write to FBO
	float lum = (0.2126 * result .r +
	             0.7152 * result .g +
				 0.0722 * result .b);
	fboColor = vec4(result, log(result + 1e-6f));
}