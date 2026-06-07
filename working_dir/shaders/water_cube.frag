#version 430
/*
	Cube variant of water.frag: reflection samples the sky cubemap directly
	by world-space direction (samplerCube) instead of an equirect sampler2D.
*/

#define IN_UV		 layout(location = 0)
#define IN_NORMAL	 layout(location = 1)
#define IN_WORLD_POS layout(location = 2)

#define OUT_FBO		layout(location = 0)

#define T_HDR	layout(binding = 0)

#define U_SUN_DIR	  layout(location = 0)
#define U_SUN_POW	  layout(location = 1)
#define U_WATER_COLOR layout(location = 2)
#define U_CAM_POS     layout(location = 3)
#define U_WATER_IOR   layout(location = 4)

in IN_UV		vec2 fUV;
in IN_NORMAL    vec3 fNormal;
in IN_WORLD_POS vec3 fWorldPos;

out OUT_FBO vec4 fboColor;

U_SUN_DIR     uniform vec3 uLDir;
U_SUN_POW     uniform float uLPow;
U_WATER_COLOR uniform vec3 uWaterColor;
U_CAM_POS     uniform vec3 uCamPos;
U_WATER_IOR	  uniform float uWaterIOR;

uniform T_HDR samplerCube tHDR;

const float cAirIOR = 1;

void main(void)
{
	vec3 N = fNormal;
	vec3 V = normalize(uCamPos - fWorldPos);
	float etaRatio = (cAirIOR - uWaterIOR) / (cAirIOR + uWaterIOR);
	etaRatio = etaRatio * etaRatio;

	float cosTheta = clamp(dot(N, V), 0, 1);
	float f = 1 - cosTheta;
	float f2 =  f * f;
	float f4 = f2 * f2;
	float f5 = f4 * f;

	float fresnelTerm = etaRatio + (1 - etaRatio) * f5;

	vec3 R = normalize(reflect(-V, N));
	vec3 hdrLum = textureLod(tHDR, R, 0.0).rgb;
	vec3 result = uWaterColor * hdrLum * fresnelTerm;

	float lum = (0.2126 * result .r +
	             0.7152 * result .g +
				 0.0722 * result .b);
	fboColor = vec4(result, log(result + 1e-6f));
}
