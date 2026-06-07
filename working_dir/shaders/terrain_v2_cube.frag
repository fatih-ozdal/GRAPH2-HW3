#version 430
/*
	Cube variant of terrain_v2.frag: IBL samples the sky cubemap directly
	by world-space direction (samplerCube) instead of an equirect sampler2D.
*/

// [DEF] In
#define IN_NORMAL	layout(location = 0)
#define IN_HEIGHT	layout(location = 1)
#define IN_POS		layout(location = 2)
#define IN_UV		layout(location = 3)
// [DEF] Out
#define OUT_FBO		layout(location = 0)
// [DEF] Textures
#define T_SNOW_ALBEDO  layout(binding = 0)
#define T_SNOW_ROUGH   layout(binding = 1)
#define T_ROCK_ALBEDO  layout(binding = 2)
#define T_ROCK_ROUGH   layout(binding = 3)
#define T_SHORE_ALBEDO layout(binding = 4)
#define T_SHORE_ROUGH  layout(binding = 5)
#define T_GRASS_ALBEDO layout(binding = 6)
#define T_GRASS_ROUGH  layout(binding = 7)
#define T_HDR		   layout(binding = 8)

// [DEF] Uniforms
#define U_RANGE		  layout(location = 0)
#define U_SUN_DIR     layout(location = 1)
#define U_WATER_LEVEL layout(location = 2)
#define U_CAM_POS	  layout(location = 3)
#define U_SUN_POW	  layout(location = 4)

in IN_NORMAL vec3  fNormal;
in IN_HEIGHT float fHeight;
in IN_POS	 vec3  fWorldPos;
in IN_UV	 vec2  fUV;

out OUT_FBO   vec4 fboColor;

U_RANGE       uniform vec2  uYRange;
U_SUN_DIR     uniform vec3  uLDir;
U_WATER_LEVEL uniform float uWaterLevel;
U_CAM_POS	  uniform vec3	uCamPos;
U_SUN_POW	  uniform float	uSunPow;

uniform T_SNOW_ALBEDO  sampler2D tSnowAlbedo;
uniform T_SNOW_ROUGH   sampler2D tSnowRough;
uniform T_ROCK_ALBEDO  sampler2D tRockAlbedo;
uniform T_ROCK_ROUGH   sampler2D tRockRough;
uniform T_SHORE_ALBEDO sampler2D tShoreAlbedo;
uniform T_SHORE_ROUGH  sampler2D tShoreRough;
uniform T_GRASS_ALBEDO sampler2D tGrassAlbedo;
uniform T_GRASS_ROUGH  sampler2D tGrassRough;
uniform T_HDR		   samplerCube tHDR;

const uint TERRAIN_GRADIENT_COUNT = 5;
const vec3 TERRAIN_COLORS[TERRAIN_GRADIENT_COUNT + 1] =
{
	vec3(0.229, 0.765, 0.049),
	vec3(0.329, 0.565, 0.149),
	vec3(0.666, 0.424, 0.027),
	vec3(0.478, 0.227, 0.027),
	vec3(1.000, 1.000, 1.000),
	vec3(1.000, 1.000, 1.000)
};

void main(void)
{
	float slopeness = abs(fNormal[1]);
	slopeness = slopeness * slopeness * slopeness;
	slopeness = 1 - slopeness;

	float heightness = fHeight - uWaterLevel;
	heightness = (heightness < 0.0f)
				? heightness / abs(uYRange[0] - uWaterLevel)
				: heightness / abs(uYRange[1] - uWaterLevel);
	heightness = clamp(heightness, -1, 1);

	vec3 albedo; float roughness;
	if(heightness < 0.0f)
	{
		albedo = texture(tShoreAlbedo, fUV).rgb;
		roughness = texture(tShoreRough, fUV).r;
	}
	else
	{
		heightness *= 2.0f;

		uint texId = uint(heightness);
		float frac = heightness - floor(heightness);
		frac = smoothstep(0, 1, pow(frac, 0.33));
		if(texId == 0)
		{
			albedo = mix(texture(tShoreAlbedo, fUV).rgb,
						 texture(tGrassAlbedo, fUV).rgb,
						 frac);
			roughness = mix(texture(tShoreRough, fUV).r,
							texture(tGrassRough, fUV).r,
							frac);
		}
		else
		{
			albedo = mix(texture(tGrassAlbedo, fUV).rgb,
						 texture(tSnowAlbedo, fUV).rgb,
						 frac);
			roughness = mix(texture(tGrassRough, fUV).r,
							texture(tSnowRough, fUV).r,
							frac);
		}
		albedo = mix(albedo, texture(tRockAlbedo, fUV).rgb * TERRAIN_COLORS[2], slopeness);
		roughness = mix(roughness, texture(tRockRough, fUV).r, slopeness);
	}

	vec3 result = vec3(0);
	vec3 V = normalize(uCamPos - fWorldPos);

	// Diffuse irradiance approximated by a high cubemap mip.
	vec3 diffuse = albedo * textureLod(tHDR, fNormal, 10.0f).rgb;

	// Roughness-driven specular reflection (~11 mips).
	float hdrLOD = roughness * roughness * 10.0f;
	vec3 reflVec = normalize(reflect(-V, fNormal));
	vec3 hdrLum = textureLod(tHDR, reflVec, hdrLOD).rgb;
	vec3 specular = albedo * hdrLum;
	result += (diffuse + specular) * 0.25f;

	vec3 sunL = -uLDir;
	vec3 sunDiff = clamp(dot(sunL , fNormal), 0, 1) * albedo * uSunPow;
	vec3 H = normalize(V + sunL);
	float specPow = roughness * roughness * 100.0f;
	float specTerm = pow(clamp(dot(H, fNormal), 0, 1), specPow);
	result += specTerm + sunDiff;

	float lum = (0.2126 * result .r +
	             0.7152 * result .g +
				 0.0722 * result .b);
	fboColor = vec4(result, log(result + 1e-6f));
}
