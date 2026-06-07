#version 430
/*
	Samples the baked sky cubemap along the raw world-space view ray.
	The ray is rebuilt from NDC in view space and rotated to world by the
	inverse view rotation; the direction is passed to texture() unmodified.
*/

#define IN_UV           layout(location = 0)

#define U_INV_VIEW      layout(location = 0)   // mat3 camera->world rotation
#define U_TAN_HALF_FOV  layout(location = 3)   // vec2 tan(fov * 0.5)

#define T_HDR	        layout(binding = 0)

in IN_UV vec2 fUV;

layout(location = 0) out vec4 fboOut;

U_INV_VIEW     uniform mat3 uInvView;
U_TAN_HALF_FOV uniform vec2 uTanHalfFov;

uniform T_HDR samplerCube tHDR;

void main()
{
	vec2 ndc = fUV * 2.0 - 1.0;
	// View space ray (camera looks down -Z), scaled by the field of view.
	vec3 viewRay = vec3(ndc.x * uTanHalfFov.x, ndc.y * uTanHalfFov.y, -1.0);
	vec3 dir = normalize(uInvView * viewRay);

	vec3 hdrColor = texture(tHDR, dir).rgb;
	float lum = (0.2126 * hdrColor.r +
	             0.7152 * hdrColor.g +
				 0.0722 * hdrColor.b);
	fboOut = vec4(hdrColor, log(lum + 1e-6f));
}
