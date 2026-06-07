#version 430

/*
	**HDR ->  SDR Tonemap Shader**

	File Name	: toneMap.frag
	Author		: Bora Yalciner
	Description	:

		Applies Reinhard 2002 Global Tonemap Operator
*/

#define IN_UV  layout(location = 0)

#define U_MID_GRAY  layout(location = 0)
#define U_PIX_COUNT layout(location = 1)
#define U_WHITE		layout(location = 2)

#define T_HDR_FRAME	layout(binding = 0)

// In
in IN_UV vec2 fUV;

// Out
out vec4 fboOut;

// Uniforms
U_MID_GRAY uniform float middleGray;
U_PIX_COUNT uniform uint pixelCount;
U_WHITE uniform float lWhite;

// Textures
uniform T_HDR_FRAME sampler2D tFrame;

void main()
{
	vec2 uv = fUV; // [ 0, 1]

	float avgLogLum = textureLod(tFrame, fUV, 20.0f).a;
	vec3 hdrColor   = texture(tFrame, fUV).rgb;
	float hdrLogLum = texture(tFrame, fUV).a;

	// Mipmap generation "divided" by pixel count
	// however we need to divide by pixel count after exponentiation
	// so we multiply then divide.
	//float avgLum = exp(avgLogLum * float(pixelCount)) / float(pixelCount);
	// TODO: https://user.ceng.metu.edu.tr/~akyuz/files/hdrgpu.pdf
	// Does not do this?
	float avgLum = exp(avgLogLum);
	float ratio = middleGray / avgLum;
	float hdrLum = exp(hdrLogLum) * ratio;
	float finalLum = hdrLum * (1 + hdrLum / (lWhite * lWhite));
	finalLum =  finalLum / (1 + hdrLum);

	// Color
	vec3 sdrColor = hdrColor * (finalLum / hdrLum);

	vec3 sdrColorGamma = pow(sdrColor, vec3(1./2.2));
	//vec3 sdrColorGamma = sdrColor;

	// Alpha is one now since this is an actual alpha
	fboOut = vec4(sdrColorGamma, 1.0f);
}
