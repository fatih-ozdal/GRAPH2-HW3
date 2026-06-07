#pragma once

#include "utility.h"
#include "hw2.h"   // Reuse Terrain_v2 / Plane / Water / SunParams as asset holders

#include <array>

struct HW3
{
    // HW2 scene objects are reused only as containers for their GL resources
    // (meshes + textures); HW3 draws them with its own samplerCube shaders.
    Terrain_v2 terrain;
    Plane      plane;
    Water      water;
    GLState&   state;

    // Source equirect HDR (input to the bake) and the baked sky+clouds cubemap.
    TextureGL  hdrEquirect;
    CubemapGL  skyCubemap;
    ShaderGL   bakeCompute;
    // Re-bake the cubemap whenever set (startup + future cloud param changes).
    bool       cubemapDirty = true;

    ShaderGL   ppVert;
    ShaderGL   skyCubeFrag;       // sky background (samplerCube)
    ShaderGL   terrainCubeFrag;
    ShaderGL   waterCubeFrag;
    ShaderGL   planeCubeFrag;
    ShaderGL   toneMapFrag;

    glm::ivec2 curFBSize = glm::ivec2(0, 0);
    GLuint     hdrFramebuffer = 0;
    GLuint     frameColorTex  = 0;
    GLuint     frameDepthTex  = 0;

    // sun.direction will later be derived from the brightest pixel of the
    // HDR equirect texture rather than hand-set.
    SunParams  sun;

    float middleGray = 0.18f;
    float lWhite     = 1.0f;

    static constexpr std::array<glm::vec2, 3> ppTri =
    {
        glm::vec2(-1, -1),
        glm::vec2( 3, -1),
        glm::vec2(-1,  3)
    };
    GLuint ppVertexBuffer = 0;
    GLuint ppVAO = 0;

         HW3(ThreadPool& threadPool, GLState& state);
         HW3(const HW3&) = delete;
         HW3(HW3&&) = delete;
    HW3& operator=(const HW3&) = delete;
    HW3& operator=(HW3&&) = delete;
         ~HW3();

    void Work();
    void ResetFramebuffer();
    // Bakes hdrEquirect into skyCubemap (compute) and regenerates its mips.
    void BakeCubemap();

    // HW3 cube-IBL draw path: reuses the HW2 components' vertex programs,
    // meshes and material textures but binds skyCubemap as a samplerCube and
    // uses the *_cube.frag shaders. HW2's own Render() methods are not used.
    void RenderBackground(const glm::mat4& view, const glm::vec2& fov);
    void RenderTerrain(const glm::mat4& view, const glm::mat4& proj);
    void RenderWater(const glm::mat4& view, const glm::mat4& proj, float deltaT);
    void RenderPlane(const glm::mat4& view, const glm::mat4& proj);
};
