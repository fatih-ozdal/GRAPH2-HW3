#version 430
/*
	**Generic Post Process Shader**

	File Name	: postProcess.vert
	Author		: Bora Yalciner
	Description	:

		Post Process Pass Shader
		Generic Vertex Shader for post process operations
*/

#define IN_POS layout(location = 0)
#define OUT_UV layout(location = 0)

// In
in IN_POS vec2 vPos;

// Out
out gl_PerVertex {vec4 gl_Position;};
out OUT_UV vec2 fUV;

void main()
{
	// Somewhat cooler way of generating post process
	// quad. We only send a single triangle as such.
	//
	//     V2(-1, 3)
	//   |\
	//   | \
	//   |  \
	//   |   \
	//   |    \
	//   |     \
	//   |      \
	//   |       \
	//   |        \
	//   |         \
	//   |__________\ (1, 1)
	//   |          |\
	//   |          | \
	//   |          |  \
	//   |__________|___\
	//   ^               ^
	//   V0(-1, -1)      V1(3, -1)
	//
	//
	// Hardware will cull the triangle and generate the pixels
	//
	// Position to UV mapping
	//		Pos					UV
	//	-1.0f,  3.0f,	-->	0.0f, 2.0f,
	//	 3.0f, -1.0f,	-->	2.0f, 0.0f,
	//	-1.0f, -1.0f,	-->	0.0f, 0.0f
	fUV = (vPos + 1.0f) * 0.5f;
	gl_Position = vec4(vPos.xy, 0.0f, 1.0f);
}
