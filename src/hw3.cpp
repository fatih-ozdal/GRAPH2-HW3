#include "hw3.h"
#include <bit>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

#include <stb_image.h>

// Replicates the compute shader's FaceDirection so the baked corner directions
// can be checked against the OpenGL cube-map spec.
static glm::vec3 FaceDirCPU(int face, float u, float v)
{
    switch(face)
    {
        case 0:  return glm::normalize(glm::vec3( 1.0f, -v, -u)); // +X
        case 1:  return glm::normalize(glm::vec3(-1.0f, -v,  u)); // -X
        case 2:  return glm::normalize(glm::vec3(  u,  1.0f,  v)); // +Y
        case 3:  return glm::normalize(glm::vec3(  u, -1.0f, -v)); // -Y
        case 4:  return glm::normalize(glm::vec3(  u, -v,  1.0f)); // +Z
        default: return glm::normalize(glm::vec3( -u, -v, -1.0f)); // -Z
    }
}

// Reads back each baked cube face (mip 0) and writes faceN.ppm (Reinhard +
// gamma, flipped so the PPM is upright) for visual inspection of the bake.
static void DumpCubeFaces(const CubemapGL& cube)
{
    int n = cube.faceSize;
    std::vector<float> buf(size_t(n) * size_t(n) * 4);
    std::vector<uint8_t> rgb(size_t(n) * size_t(n) * 3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube.textureId);
    for(int f = 0; f < 6; ++f)
    {
        glGetTexImage(GLenum(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f), 0,
                      GL_RGBA, GL_FLOAT, buf.data());
        for(int y = 0; y < n; ++y)
        for(int x = 0; x < n; ++x)
        {
            const float* p = &buf[(size_t(y) * size_t(n) + size_t(x)) * 4];
            int oy = n - 1 - y; // PPM is top-to-bottom
            uint8_t* o = &rgb[(size_t(oy) * size_t(n) + size_t(x)) * 3];
            for(int c = 0; c < 3; ++c)
            {
                float v = p[c];
                v = v / (v + 1.0f);              // Reinhard
                v = std::pow(v, 1.0f / 2.2f);    // gamma
                o[c] = uint8_t(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
        char name[32];
        std::snprintf(name, sizeof(name), "face%d.ppm", f);
        if(std::FILE* fp = std::fopen(name, "wb"))
        {
            std::fprintf(fp, "P6\n%d %d\n255\n", n, n);
            std::fwrite(rgb.data(), 1, rgb.size(), fp);
            std::fclose(fp);
            std::printf("[BAKE-DEBUG] wrote %s\n", name);
        }
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static void LogFaceCorners(int faceSize)
{
    static const char* const names[6] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
    // GL-spec corner direction at texel (0,0) (in-face u,v -> -1), normalized.
    static const glm::vec3 expected[6] =
    {
        glm::normalize(glm::vec3( 1,  1,  1)), // +X
        glm::normalize(glm::vec3(-1,  1, -1)), // -X
        glm::normalize(glm::vec3(-1,  1, -1)), // +Y
        glm::normalize(glm::vec3(-1, -1,  1)), // -Y
        glm::normalize(glm::vec3(-1,  1,  1)), // +Z
        glm::normalize(glm::vec3( 1,  1, -1)), // -Z
    };
    float c = (0.5f) / float(faceSize) * 2.0f - 1.0f; // u == v at texel (0,0)
    for(int f = 0; f < 6; ++f)
    {
        glm::vec3 a = FaceDirCPU(f, c, c);
        const glm::vec3& e = expected[f];
        std::printf("[BAKE-DEBUG] face %d (%s) (0,0): actual=(%.3f, %.3f, %.3f)  expected=(%.3f, %.3f, %.3f)\n",
                    f, names[f], a.x, a.y, a.z, e.x, e.y, e.z);
    }
}

static const char* const HdrPath = "textures/qwantani_mid_morning_puresky_2k.hdr";

// Brightest equirect pixel -> the direction light travels (scene shaders use
// -uLDir as the light vector). Inverts the same mapping the bake compute uses.
static glm::vec3 SunLightDirFromHDR(const char* path)
{
    stbi_set_flip_vertically_on_load(1);
    int w = 0, h = 0, c = 0;
    float* px = stbi_loadf(path, &w, &h, &c, 3);
    if(!px) return glm::normalize(glm::vec3(0.5f, -0.5f, 0.0f));

    float best = -1.0f;
    int bx = 0, by = 0;
    for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
    {
        const float* p = px + (size_t(y) * size_t(w) + size_t(x)) * 3;
        float lum = 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
        if(lum > best) { best = lum; bx = x; by = y; }
    }
    stbi_image_free(px);

    float u = (float(bx) + 0.5f) / float(w);
    float v = (float(by) + 0.5f) / float(h);
    float azimuth = u * 2.0f * glm::pi<float>() - glm::pi<float>();  // atan(x,z)
    float incl    = (1.0f - v) * glm::pi<float>();                   // 1 - incl/PI
    float r = glm::sin(incl);
    glm::vec3 toSun = glm::vec3(r * glm::sin(azimuth),
                                glm::cos(incl),
                                r * glm::cos(azimuth));
    return glm::normalize(-toSun);
}

HW3::HW3(ThreadPool& threadPool, GLState& state)
    : terrain(threadPool,
              TerrainMeshGenerationParams
              {
                .rangeX = glm::vec2(-600, 600),
                .rangeY = glm::vec2(-50, 50),
                .rangeZ = glm::vec2(-600, 600),
                .patchStartOffset = glm::uvec2(0),
                .patchCount = glm::uvec2(256),
                .controlPointPerPatch = glm::uvec2(4),
                .vertexPerPatch = glm::uvec2(4),
                .controlPointSkip = glm::uvec2(10)
              })
    , plane()
    , water(-39.0f,
            PlaneGenParams
            {
                .rangeX = glm::vec2(-700, 700),
                .rangeZ = glm::vec2(-700, 700),
                .vertexCount = glm::vec2(1024, 1024)
            })
    , state(state)
    , hdrEquirect(HdrPath, TextureGL::LINEAR, TextureGL::REPEAT)
    , skyCubemap(1024)
    , bakeCompute(ShaderGL::COMPUTE, "shaders/equirectToCubemap.comp")
    , ppVert(ShaderGL::VERTEX, "shaders/postProcess.vert")
    , skyCubeFrag(ShaderGL::FRAGMENT, "shaders/hdr_cube.frag")
    , terrainCubeFrag(ShaderGL::FRAGMENT, "shaders/terrain_v2_cube.frag")
    , waterCubeFrag(ShaderGL::FRAGMENT, "shaders/water_cube.frag")
    , planeCubeFrag(ShaderGL::FRAGMENT, "shaders/plane_cube.frag")
    , toneMapFrag(ShaderGL::FRAGMENT, "shaders/toneMap.frag")
{
    plane.position = state.cam.gaze;

    glGenBuffers(1, &ppVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, ppVertexBuffer);
    glBufferStorage(GL_ARRAY_BUFFER, GLintptr(6 * sizeof(float)),
                    nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(0), GLsizei(6 * sizeof(float)),
                    ppTri.data());
    //
    glGenVertexArrays(1, &ppVAO);
    glBindVertexArray(ppVAO);
    glBindVertexBuffer(0, ppVertexBuffer, GLintptr(0), GLsizei(sizeof(glm::vec2)));
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexArray(0);

    sun.direction = SunLightDirFromHDR(HdrPath);
    LogFaceCorners(skyCubemap.faceSize);
}

void HW3::BakeCubemap()
{
    glUseProgramStages(state.renderPipeline, GL_COMPUTE_SHADER_BIT, bakeCompute.shaderId);
    glActiveShaderProgram(state.renderPipeline, bakeCompute.shaderId);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrEquirect.textureId);
    glBindImageTexture(0, skyCubemap.textureId, 0, GL_TRUE, 0,
                       GL_WRITE_ONLY, GL_RGBA32F);

    GLuint groups = GLuint(skyCubemap.faceSize) / 8u;
    glDispatchCompute(groups, groups, 6);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    glBindTexture(GL_TEXTURE_CUBE_MAP, skyCubemap.textureId);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Release the image binding so the cube is not left aliased as a writable
    // image while it is sampled as a texture during rendering.
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
}

void HW3::RenderBackground(const glm::mat4& view, const glm::vec2& fov)
{
    static constexpr GLuint U_INV_VIEW     = 0;
    static constexpr GLuint U_TAN_HALF_FOV = 3;
    static constexpr GLuint T_HDR          = 0;

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, ppVert.shaderId);
    glActiveShaderProgram(state.renderPipeline, ppVert.shaderId);
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, skyCubeFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, skyCubeFrag.shaderId);
    {
        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyCubemap.textureId);

        // mat3(view) is the orthonormal world->view rotation, so its inverse
        // (view->world) is just its transpose.
        glm::mat3 invView = glm::transpose(glm::mat3(view));
        glm::vec2 tanHalfFov = glm::vec2(std::tan(fov[0] * 0.5f),
                                         std::tan(fov[1] * 0.5f));
        glUniformMatrix3fv(U_INV_VIEW, 1, GL_FALSE, glm::value_ptr(invView));
        glUniform2fv(U_TAN_HALF_FOV, 1, glm::value_ptr(tanHalfFov));
    }
    glBindVertexArray(ppVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void HW3::RenderTerrain(const glm::mat4& view, const glm::mat4& proj)
{
    static constexpr GLuint U_TRANSFORM_MODEL     = 0;
    static constexpr GLuint U_TRANSFORM_VIEW      = 1;
    static constexpr GLuint U_TRANSFORM_PROJ      = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL    = 3;
    static constexpr GLuint U_VERTEX_HEIGHT_SCALE = 4;
    //
    static constexpr GLuint U_RANGE       = 0;
    static constexpr GLuint U_SUN_DIR     = 1;
    static constexpr GLuint U_WATER_LEVEL = 2;
    static constexpr GLuint U_CAM_POS     = 3;
    static constexpr GLuint U_SUN_POW     = 4;
    //
    static constexpr GLuint T_SNOW_ALBEDO  = 0;
    static constexpr GLuint T_SNOW_ROUGH   = 1;
    static constexpr GLuint T_ROCK_ALBEDO  = 2;
    static constexpr GLuint T_ROCK_ROUGH   = 3;
    static constexpr GLuint T_SHORE_ALBEDO = 4;
    static constexpr GLuint T_SHORE_ROUGH  = 5;
    static constexpr GLuint T_GRASS_ALBEDO = 6;
    static constexpr GLuint T_GRASS_ROUGH  = 7;
    static constexpr GLuint T_HDR          = 8;

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, terrain.vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, terrain.vert.shaderId);
    {
        glm::mat4x4 model = glm::identity<glm::mat4x4>();
        glm::mat3x3 normalMatrix = glm::inverseTranspose(model);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(model));
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
        glUniform1f(U_VERTEX_HEIGHT_SCALE, 1.0f);
    }
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, terrainCubeFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, terrainCubeFrag.shaderId);
    {
        glUniform2fv(U_RANGE, 1, glm::value_ptr(terrain.terrain.yMinMax));
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform1f(U_WATER_LEVEL, water.waterElevation.y);
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));

        auto SetTex = [](GLuint index, const TextureGL& tex)
        {
            glActiveTexture(GL_TEXTURE0 + index);
            glBindTexture(GL_TEXTURE_2D, tex.textureId);
        };
        SetTex(T_SNOW_ALBEDO, terrain.snowAlbedo);
        SetTex(T_SNOW_ROUGH, terrain.snowRoughness);
        SetTex(T_ROCK_ALBEDO, terrain.rockAlbedo);
        SetTex(T_ROCK_ROUGH, terrain.rockRoughness);
        SetTex(T_SHORE_ALBEDO, terrain.shoreAlbedo);
        SetTex(T_SHORE_ROUGH, terrain.shoreRoughness);
        SetTex(T_GRASS_ALBEDO, terrain.grassAlbedo);
        SetTex(T_GRASS_ROUGH, terrain.grassRoughness);
        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyCubemap.textureId);
    }
    glBindVertexArray(terrain.terrain.vaoId);
    glDrawElements(GL_TRIANGLES, terrain.terrain.indexCount, GL_UNSIGNED_INT, nullptr);
}

void HW3::RenderWater(const glm::mat4& view, const glm::mat4& proj, float deltaT)
{
    static constexpr GLuint U_TRANSFORM_MODEL  = 0;
    static constexpr GLuint U_TRANSFORM_VIEW   = 1;
    static constexpr GLuint U_TRANSFORM_PROJ   = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL = 3;
    static constexpr GLuint U_TIME             = 4;
    //
    static constexpr GLuint U_SUN_DIR     = 0;
    static constexpr GLuint U_SUN_POW     = 1;
    static constexpr GLuint U_WATER_COLOR = 2;
    static constexpr GLuint U_CAM_POS     = 3;
    static constexpr GLuint U_WATER_IOR   = 4;
    //
    static constexpr GLuint T_HDR = 0;

    water.timeElapsed += deltaT;

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, water.vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, water.vert.shaderId);
    {
        glm::mat4x4 model = glm::translate(glm::identity<glm::mat4x4>(), water.waterElevation);
        glm::mat3x3 normalMatrix = glm::inverseTranspose(model);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(model));
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
        glUniform1f(U_TIME, water.timeElapsed);
    }
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, waterCubeFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, waterCubeFrag.shaderId);
    {
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform3fv(U_WATER_COLOR, 1, glm::value_ptr(water.waterColor));
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));
        glUniform1f(U_WATER_IOR, water.waterIoR);

        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyCubemap.textureId);
    }
    glBindVertexArray(water.mesh.vaoId);
    glDrawElements(GL_TRIANGLES, water.mesh.indexCount, GL_UNSIGNED_INT, nullptr);
}

void HW3::RenderPlane(const glm::mat4& view, const glm::mat4& proj)
{
    static constexpr GLuint U_TRANSFORM_MODEL  = 0;
    static constexpr GLuint U_TRANSFORM_VIEW   = 1;
    static constexpr GLuint U_TRANSFORM_PROJ   = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL = 3;
    //
    static constexpr GLuint U_MODE    = 0;
    static constexpr GLuint U_SUN_DIR = 1;
    static constexpr GLuint U_SUN_POW = 2;
    static constexpr GLuint U_CAM_POS = 3;
    //
    static constexpr GLuint T_BODY_ALBEDO  = 0;
    static constexpr GLuint T_BODY_ROUGH   = 1;
    static constexpr GLuint T_HELIX_ALBEDO = 2;
    static constexpr GLuint T_HELIX_ROUGH  = 3;
    static constexpr GLuint T_HDR          = 4;

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, plane.vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, plane.vert.shaderId);
    {
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));
    }
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, planeCubeFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, planeCubeFrag.shaderId);
    {
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));

        auto SetTex = [](GLuint index, const TextureGL& tex)
        {
            glActiveTexture(GL_TEXTURE0 + index);
            glBindTexture(GL_TEXTURE_2D, tex.textureId);
        };
        SetTex(T_BODY_ALBEDO, plane.bodyAlbedoTex);
        SetTex(T_BODY_ROUGH, plane.bodyRoughnessTex);
        SetTex(T_HELIX_ALBEDO, plane.helixAlbedoTex);
        SetTex(T_HELIX_ROUGH, plane.helixRoughnessTex);
        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyCubemap.textureId);
    }

    enum RenderMode : unsigned int { BODY = 0, HELIX = 1, CABLE = 2, GLASS = 3 };

    glm::mat4x4 r = glm::toMat4(plane.rotation);
    glm::mat4x4 t = glm::translate(glm::identity<glm::mat4x4>(), plane.position);
    glm::mat4x4 s = glm::scale(glm::identity<glm::mat4x4>(), glm::vec3(0.3f));
    glm::mat4x4 model = t * s * r;

    auto DrawPart = [&](const glm::mat4x4& partModel, RenderMode mode, const MeshGL& mesh)
    {
        glm::mat3x3 normalMatrix = glm::inverseTranspose(partModel);
        glActiveShaderProgram(state.renderPipeline, plane.vert.shaderId);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(partModel));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
        glActiveShaderProgram(state.renderPipeline, planeCubeFrag.shaderId);
        glUniform1ui(U_MODE, mode);
        glBindVertexArray(mesh.vaoId);
        glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    };

    DrawPart(model, BODY, plane.bodyMesh);

    glm::mat4x4 localRot = glm::rotate(glm::identity<glm::mat4x4>(), plane.propRotation, glm::vec3(0, 0, 1));
    glm::mat4x4 helixModel = model * glm::translate(localRot, Plane::localHelixTranslate);
    DrawPart(helixModel, HELIX, plane.helixMesh);

    glm::mat4x4 cableModel = model * glm::translate(glm::identity<glm::mat4x4>(), Plane::localCableTranslate);
    DrawPart(cableModel, CABLE, plane.cablesMesh);

    glm::mat4x4 glassModel = model * glm::translate(glm::identity<glm::mat4x4>(), Plane::localGlassTranslate);
    DrawPart(glassModel, GLASS, plane.glassMesh);
}

void HW3::ResetFramebuffer()
{
    if(hdrFramebuffer) glDeleteFramebuffers(1, &hdrFramebuffer);
    if(frameColorTex)  glDeleteTextures(1, &frameColorTex);
    if(frameDepthTex)  glDeleteTextures(1, &frameDepthTex);
    hdrFramebuffer = 0;
    frameColorTex = 0;
    frameDepthTex = 0;

    glGenFramebuffers(1, &hdrFramebuffer);
    glGenTextures(1, &frameColorTex);
    glGenTextures(1, &frameDepthTex);
    //
    glBindTexture(GL_TEXTURE_2D, frameColorTex);
    uint32_t mipCount = uint32_t(std::max(state.curWndParams.fbSize[0],
                                          state.curWndParams.fbSize[1]));
    mipCount = (sizeof(GLsizei) * CHAR_BIT) - uint32_t(std::countl_zero(mipCount));
    glTexStorage2D(GL_TEXTURE_2D, GLsizei(mipCount), GL_RGBA32F,
                   state.curWndParams.fbSize[0], state.curWndParams.fbSize[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //
    glBindTexture(GL_TEXTURE_2D, frameDepthTex);
    glTexStorage2D(GL_TEXTURE_2D, GLsizei(1), GL_DEPTH24_STENCIL8,
                   state.curWndParams.fbSize[0], state.curWndParams.fbSize[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrFramebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, frameColorTex, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, frameDepthTex, 0);
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::fprintf(stderr, "Incomplete framebuffer!\n");
        std::exit(EXIT_FAILURE);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

int x = 31;

void HW3::Work()
{
    static float lastTime = 0;
    float curTime = float(glfwGetTime());
    float deltaT = curTime - lastTime;
    lastTime = curTime;

    plane.ChangePitch(state.pitch); state.pitch = 0.0f;
    plane.ChangeRoll(state.roll); state.roll = 0.0f;
    plane.ChangeYaw(state.yaw); state.yaw = 0.0f;
    plane.ChangeVelocity(state.speedModifier); state.speedModifier = 0.0f;
    plane.Update(state, deltaT);

    // Track the plane
    glm::vec3 deltaVec = plane.position - state.cam.gaze;
    state.cam.gaze = plane.position;
    state.cam.pos += deltaVec;

    if(state.curWndParams.fbSize[0] == 0 ||
       state.curWndParams.fbSize[1] == 0)
        return;

    if(cubemapDirty)
    {
        BakeCubemap();
        DumpCubeFaces(skyCubemap); // temporary debug
        cubemapDirty = false;
    }

    x++;

    if (x == 5000)
    {
        BakeCubemap();
        DumpCubeFaces(skyCubemap); // temporary debug
        cubemapDirty = false;
    }

    // Object-common matrices
    float aspectRatio = float(state.curWndParams.fbSize[0]) / float(state.curWndParams.fbSize[1]);
    float fovY = glm::radians(50.0f);
    glm::vec2 nearFar (0.1f, 500.0f);
    glm::vec2 fov = glm::vec2(2.0f * std::atan(std::tan(fovY * 0.5f) * aspectRatio),
                              fovY);
    glm::mat4x4 proj = glm::perspective(fovY, aspectRatio, nearFar[0], nearFar[1]);
    glm::mat4x4 view = glm::lookAt(state.cam.pos, state.cam.gaze, state.cam.up);

    if(curFBSize != state.curWndParams.fbSize)
    {
        ResetFramebuffer();
        curFBSize = state.curWndParams.fbSize;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrFramebuffer);
    glViewport(0, 0, curFBSize[0], curFBSize[1]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);

    // Sky background
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    RenderBackground(view, fov);

    // Scene (cube IBL)
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    RenderTerrain(view, proj);
    RenderWater(view, proj, deltaT);
    RenderPlane(view, proj);

    // Present (tonemap)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, curFBSize[0], curFBSize[1]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, frameColorTex);
    glGenerateMipmap(GL_TEXTURE_2D);

    static constexpr GLuint U_MID_GRAY  = 0;
    static constexpr GLuint U_PIX_COUNT = 1;
    static constexpr GLuint U_WHITE     = 2;
    static constexpr GLuint T_HDR_FRAME = 0;

    static bool hdrIsOpen = true;
    ImGui::Begin("HDR Params", &hdrIsOpen);
    ImGui::SliderFloat("Middle Gray", &middleGray, 0.001f, 1.0f);
    ImGui::SliderFloat("LWhite (x10^6)", &lWhite, 0.01f, 10.0f);
    ImGui::End();

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, ppVert.shaderId);
    glActiveShaderProgram(state.renderPipeline, ppVert.shaderId);
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, toneMapFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, toneMapFrag.shaderId);
    {
        glActiveTexture(GL_TEXTURE0 + T_HDR_FRAME);
        glBindTexture(GL_TEXTURE_2D, frameColorTex);

        glUniform1f(U_MID_GRAY, middleGray);
        glUniform1ui(U_PIX_COUNT, uint32_t(curFBSize[0] * curFBSize[1]));
        glUniform1f(U_WHITE, lWhite * 1'000'000);
    }
    glBindVertexArray(ppVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

HW3::~HW3()
{
    if(hdrFramebuffer) glDeleteFramebuffers(1, &hdrFramebuffer);
    if(frameColorTex) glDeleteTextures(1, &frameColorTex);
    if(frameDepthTex) glDeleteTextures(1, &frameDepthTex);
    hdrFramebuffer = 0;
    frameColorTex = 0;
    frameDepthTex = 0;
    //
    if(ppVertexBuffer) glDeleteBuffers(1, &ppVertexBuffer);
    if(ppVAO)          glDeleteVertexArrays(1, &ppVAO);
    ppVertexBuffer = 0;
    ppVAO = 0;
}
