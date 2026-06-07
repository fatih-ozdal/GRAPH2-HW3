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
#define T_BODY_ALBEDO	layout(binding = 0)
#define T_BODY_ROUGH	layout(binding = 1)
#define T_HELIX_ALBEDO	layout(binding = 2)
#define T_HELIX_ROUGH	layout(binding = 3)
#define T_HDR			layout(binding = 4)

// This must match the first parameter of glUniform...() calls
#define U_MODE		layout(location = 0)
#define U_SUN_DIR	layout(location = 1)
#define U_SUN_POW	layout(location = 2)
#define U_CAM_POS   layout(location = 3)

// Input
in IN_UV	    vec2 fUV;
in IN_NORMAL    vec3 fNormal;
in IN_WORLD_POS vec3 fWorldPos;

// Output
// This parameter goes to the framebuffer
out OUT_FBO vec4 fboColor;

// Uniforms
U_MODE    uniform uint uMode;
U_SUN_DIR uniform vec3 uLDir;
U_SUN_POW uniform float uLPow;
U_CAM_POS uniform vec3 uCamPos;

// Textures
uniform T_BODY_ALBEDO  sampler2D tBodyAlbedo;
uniform T_BODY_ROUGH   sampler2D tBodyRough;
uniform T_HELIX_ALBEDO sampler2D tHelixAlbedo;
uniform T_HELIX_ROUGH  sampler2D tHelixRough;
uniform T_HDR		   sampler2D tHDR;

// Globals
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
	uint mode = uMode;

	vec3 albedo; float roughness;
	switch(mode)
	{
		// Body
		case 0:
			albedo = texture(tBodyAlbedo, fUV).rgb;
			roughness = texture(tBodyRough, fUV).r;
			break;
		// Helix
		case 1:
			albedo = texture(tHelixAlbedo, fUV).rgb;
			roughness = texture(tHelixRough, fUV).r;
			break;
		// Cable
		default:
		case 2:
			albedo = vec3(0.5);
			roughness = 1.0;
			break;
		// Glass
		case 3:
			albedo = vec3(1);
			roughness = 0.01f;
			break;
	}

	vec3 N = fNormal;
	vec3 V = normalize(uCamPos - fWorldPos);

	// Shading
	vec3 result = vec3(0);

	// Shading (Lambertian Diffuse)
	vec2 diffUV = DirToSphericalUV(fNormal);
	vec3 diffuse = albedo * textureLod(tHDR, diffUV, 10.0f).rgb;

	// Pong-Like Specular
	// We have around 11 mips
	float hdrLOD = roughness * roughness * 10.0f;
	vec3 reflVec = normalize(reflect(-V, fNormal));
	vec2 reflUV = DirToSphericalUV(reflVec);
	vec3 hdrLum = textureLod(tHDR, reflUV, hdrLOD).rgb;
	vec3 specular = albedo * hdrLum;
	result += (diffuse + specular) * 0.25f;

//	// Sun
//	vec3 sunL = -uLDir;
//	vec3 sunDiff = clamp(dot(sunL , fNormal), 0, 1) * albedo * uLPow;
//	vec3 H = normalize(V + sunL);
//	float specPow = roughness * roughness * 100.0f;
//	float specTerm = pow(clamp(dot(H, fNormal), 0, 1), specPow);
//	result += specTerm + sunDiff;

	// HDR Out
	float lum = (0.2126 * result .r +
	             0.7152 * result .g +
				 0.0722 * result .b);
	fboColor = vec4(result, log(lum + 1e-6f));
}