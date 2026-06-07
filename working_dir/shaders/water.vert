#version 430
/*
	File Name	: generic.vert
	Author		: Bora Yalciner
	Description	:

		Basic vertex transform shader...
		Just multiplies each vertex to MVP matrix
		and sends it to the rasterizer

		It supports custom per-vertex normals
		and texture (uv) coordinates.
*/

// Definitions
#define IN_POS			layout(location = 0)
#define IN_NORMAL		layout(location = 1)

#define OUT_UV			layout(location = 0)
#define OUT_NORMAL		layout(location = 1)
#define OUT_WORLD_POS   layout(location = 2)

#define U_TRANSFORM_MODEL	layout(location = 0)
#define U_TRANSFORM_VIEW	layout(location = 1)
#define U_TRANSFORM_PROJ	layout(location = 2)
#define U_TRANSFORM_NORMAL	layout(location = 3)
#define U_TIME				layout(location = 4)

// Input
in IN_POS	 vec3 vPos;
in IN_NORMAL vec3 vNormal;

// Output
// This parameter goes to rasterizer
// This is mandatory since we are using modern pipeline
out gl_PerVertex {vec4 gl_Position;};

// These pass through to rasterizer and will be iterpolated at
// fragment positions
out OUT_UV		  vec2 fUV;
out OUT_NORMAL	  vec3 fNormal;
out OUT_WORLD_POS vec3 fWorldPos;

// Uniforms
U_TRANSFORM_MODEL	uniform mat4 uModel;
U_TRANSFORM_VIEW	uniform mat4 uView;
U_TRANSFORM_PROJ	uniform mat4 uProjection;
U_TRANSFORM_NORMAL  uniform mat3 uNormalMatrix;
U_TIME				uniform float uTime;

float TrigWave(vec3 pos, float mag, float freq, float time)
{
	float result = (sin((freq * pos.x) + time) *
				    cos((0.8 * freq * pos.y) + time)
					* mag);
	return result;
}

void main(void)
{
	float time = uTime * 1.18;
	float mag = 0.08;
	//float freq = 0.25;
	float freq = 0.55;

	vec3 localPos = vPos.xyz;
	vec3 adjPosX = localPos.xyz + vec3(1.2, 0.0, 0.0);
	vec3 adjPosZ = localPos.xyz + vec3(0.0, 0.0, 1.2);

	localPos.y = TrigWave(localPos, mag , freq, time);
	adjPosX.y = TrigWave(adjPosX, mag , freq, time);
	adjPosZ.y = TrigWave(adjPosZ, mag , freq, time);

	// Normal Calcuation
	vec3 tangent = normalize(adjPosX - localPos);
	vec3 biTangent = normalize(adjPosZ - localPos);
	vec3 normal = cross(tangent, biTangent);

	fNormal = normalize(uNormalMatrix * normal);

	vec3 worldPos = (uModel * vec4(localPos, 1.0f)).xyz;
	fWorldPos = worldPos;
	// Rasterizer
	gl_Position = uProjection * uView * vec4(worldPos, 1.0f);
}