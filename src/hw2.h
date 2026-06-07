#pragma once

#include "utility.h"
#include "hw1.h"

#include <array>

struct SunParams
{
    float power = 5;
    glm::vec3 direction = glm::normalize(glm::vec3(0.5, -0.5, 0));
};

struct Plane
{
    ShaderGL vert;
    ShaderGL frag;
    //
    MeshGL   bodyMesh;
    MeshGL   helixMesh;
    MeshGL   glassMesh;
    MeshGL   cablesMesh;
    //
    float     propRotation = 0.0f;
    float     velocity     = 0.0f;
    glm::quat rotation = glm::identity<glm::quat>();
    glm::vec3 position = glm::vec3(0, 0, 5);
    //
    TextureGL bodyAlbedoTex;
    TextureGL bodyRoughnessTex;
    TextureGL helixAlbedoTex;
    TextureGL helixRoughnessTex;

    static constexpr glm::vec3 localHelixTranslate = glm::vec3(0, 0, 13.719);
    static constexpr glm::vec3 localGlassTranslate = glm::vec3(0, 2.733, -1.489);
    static constexpr glm::vec3 localCableTranslate = glm::vec3(0, 3.644, -10.638);

    // Constructors & Destructor
            Plane();
            Plane(const Plane&) = delete;
            Plane(Plane&&) = delete;
    Plane&  operator=(const Plane&) = delete;
    Plane&  operator=(Plane&&) = delete;
            ~Plane() = default;

    // Member Functions
    void Update(const GLState& state, float deltaT);
    void Render(const GLState& state,
                const TextureGL& hdr,
                const SunParams& sun,
                const glm::mat4& view,
                const glm::mat4& proj);

    void ChangeYaw(float delta);
    void ChangePitch(float delta);
    void ChangeRoll(float delta);
    void ChangeVelocity(float deltaV);
};

struct Water
{
    ShaderGL vert;
    ShaderGL frag;
    //
    glm::vec3 waterColor;
    glm::vec3 waterElevation;
    MeshGL    mesh;
    float     timeElapsed = 0.0f;
    float     waterIoR    = 1.3f;
    // Constructors & Destructor
            Water(float waterElevation,
                  const PlaneGenParams& planeParams);
            Water(const Water&) = delete;
            Water(Water&&) = delete;
    Water&  operator=(const Water&) = delete;
    Water&  operator=(Water&&) = delete;
            ~Water() = default;

    void Render(const GLState& state,
                const TextureGL& hdr,
                const SunParams& sun,
                float deltaT,
                const glm::mat4& view,
                const glm::mat4& proj);
};

struct Terrain_v2
{
    TerrainMesh terrain;
    ShaderGL    vert;
    ShaderGL    frag;

    TextureGL snowAlbedo;
    TextureGL snowRoughness;
    TextureGL rockAlbedo;
    TextureGL rockRoughness;
    TextureGL shoreAlbedo;
    TextureGL shoreRoughness;
    TextureGL grassAlbedo;
    TextureGL grassRoughness;

    // Constructors & Destructor
                Terrain_v2(ThreadPool& threadPool,
                           const TerrainMeshGenerationParams&);
                Terrain_v2(const Terrain_v2&) = delete;
                Terrain_v2(Terrain_v2&&) = delete;
    Terrain_v2& operator=(const Terrain_v2&) = delete;
    Terrain_v2& operator=(Terrain_v2&&) = delete;
                ~Terrain_v2() = default;

    void Render(const GLState& state,
                const TextureGL& hdr,
                const SunParams& sun,
                const glm::mat4& view,
                const glm::mat4& proj,
                float waterLevel);
};

struct HW2
{
    Terrain_v2 terrain;
    Plane      plane;
    Water      water;
    GLState&   state;

    TextureGL  hdr;
    ShaderGL   ppVert;
    ShaderGL   hdrFrag;
    ShaderGL   toneMapFrag;

    glm::ivec2 curFBSize = glm::uvec2(0, 0);
    GLuint     hdrFramebuffer = 0;
    GLuint     frameColorTex  = 0;
    GLuint     frameDepthTex  = 0;

    //
    SunParams  sun;

    // Tonemap Parameters
    float middleGray = 0.18f;
    float lWhite     = 1.0f;

    // Post Process Related
    static constexpr std::array<glm::vec2, 3> ppTri =
    {
        glm::vec2(-1, -1),
        glm::vec2( 3, -1),
        glm::vec2(-1,  3)
    };
    GLuint ppVertexBuffer = 0;
    GLuint ppVAO = 0;

    // Constructors & Destructor
         HW2(ThreadPool& threadPool, GLState& state);
         HW2(const HW2&) = delete;
         HW2(HW2&&) = delete;
    HW2& operator=(const HW2&) = delete;
    HW2& operator=(HW2&&) = delete;
         ~HW2();

    void Work();
    void ResetFramebuffer();

};