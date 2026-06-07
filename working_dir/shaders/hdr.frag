#version 430
/*
	**HDR Map Write Shader**

	File Name	: hdr.frag
	Author		: Bora Yalciner
	Description	:

		Directly writes the HDR map to FB
		according to the view direction
*/

#define IN_UV  layout(location = 0)

#define U_VIEW_DIR   layout(location = 0)
#define U_VIEW_UP    layout(location = 1)
#define U_VIEW_RIGHT layout(location = 2)
#define U_FOV	     layout(location = 3)
#define U_NEAR	     layout(location = 4)

#define T_HDR	   layout(binding = 0)

// In
in IN_UV vec2 fUV;

// Out
out vec4 fboOut;

// Uniforms
U_VIEW_DIR   uniform vec3 viewDir;
U_VIEW_UP    uniform vec3 viewUp;
U_VIEW_RIGHT uniform vec3 viewRight;
U_FOV	     uniform vec2 fov;       // FoV XY (Radians)
U_NEAR	     uniform float near;

// Textures
uniform T_HDR sampler2D tHDR;

#define PI 3.14159265
#define INV_PI 0.3183099

void main()
{
	vec2 uv = fUV;			  // [ 0, 1]
	float widthHalf = tan(fov[0] * 0.5) * near;
	float heightHalf = tan(fov[1] * 0.5) * near;

	vec3 bottomLeft = (-viewRight * widthHalf +
	                   -viewUp * heightHalf +
					   viewDir * near);

	vec2 planeSize = vec2(widthHalf, heightHalf) * 2;
	vec2 pixelDist = uv * planeSize;

	vec3 dir = bottomLeft;
	dir += pixelDist.x * viewRight;
	dir += pixelDist.y * viewUp;
	dir = normalize(dir);

	// Center azimuth / inclination
	float azimuth = atan(dir.x, dir.z);
	float incl = acos(clamp(dir.y, -1, 1));

	vec2 hdrUV = vec2((azimuth + PI) * 0.5 * INV_PI,
					  1.0f - incl * INV_PI);

	vec3 hdrColor = texture(tHDR, hdrUV).rgb;
	float lum = (0.2126 * hdrColor.r +
	             0.7152 * hdrColor.g +
				 0.0722 * hdrColor.b);
	fboOut = vec4(hdrColor, log(lum + 1e-6f));
}
