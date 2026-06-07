#version 430
/*
	Cube variant of plane.frag: IBL samples the sky cubemap directly by
	world-space direction (samplerCube) instead of an equirect sampler2D.
*/

#define IN_UV		 layout(location = 0)
#define IN_NORMAL	 layout(location = 1)
#define IN_WORLD_POS layout(location = 2)

#define OUT_FBO		layout(location = 0)

#define T_BODY_ALBEDO	layout(binding = 0)
#define T_BODY_ROUGH	layout(binding = 1)
#define T_HELIX_ALBEDO	layout(binding = 2)
#define T_HELIX_ROUGH	layout(binding = 3)
#define T_HDR			layout(binding = 4)

#define U_MODE		layout(location = 0)
#define U_SUN_DIR	layout(location = 1)
#define U_SUN_POW	layout(location = 2)
#define U_CAM_POS   layout(location = 3)

in IN_UV	    vec2 fUV;
in IN_NORMAL    vec3 fNormal;
in IN_WORLD_POS vec3 fWorldPos;

out OUT_FBO vec4 fboColor;

U_MODE    uniform uint uMode;
U_SUN_DIR uniform vec3 uLDir;
U_SUN_POW uniform float uLPow;
U_CAM_POS uniform vec3 uCamPos;

uniform T_BODY_ALBEDO  sampler2D tBodyAlbedo;
uniform T_BODY_ROUGH   sampler2D tBodyRough;
uniform T_HELIX_ALBEDO sampler2D tHelixAlbedo;
uniform T_HELIX_ROUGH  sampler2D tHelixRough;
uniform T_HDR		   samplerCube tHDR;

void main(void)
{
	uint mode = uMode;

	vec3 albedo; float roughness;
	switch(mode)
	{
		case 0:
			albedo = texture(tBodyAlbedo, fUV).rgb;
			roughness = texture(tBodyRough, fUV).r;
			break;
		case 1:
			albedo = texture(tHelixAlbedo, fUV).rgb;
			roughness = texture(tHelixRough, fUV).r;
			break;
		default:
		case 2:
			albedo = vec3(0.5);
			roughness = 1.0;
			break;
		case 3:
			albedo = vec3(1);
			roughness = 0.01f;
			break;
	}

	vec3 N = fNormal;
	vec3 V = normalize(uCamPos - fWorldPos);

	vec3 result = vec3(0);

	// Diffuse irradiance approximated by a high cubemap mip.
	vec3 diffuse = albedo * textureLod(tHDR, fNormal, 10.0f).rgb;

	// Roughness-driven specular reflection (~11 mips).
	float hdrLOD = roughness * roughness * 10.0f;
	vec3 reflVec = normalize(reflect(-V, fNormal));
	vec3 hdrLum = textureLod(tHDR, reflVec, hdrLOD).rgb;
	vec3 specular = albedo * hdrLum;
	result += (diffuse + specular) * 0.25f;

	float lum = (0.2126 * result .r +
	             0.7152 * result .g +
				 0.0722 * result .b);
	fboColor = vec4(result, log(lum + 1e-6f));
}
