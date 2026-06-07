#include "hw3.h"
#include <bit>

#include <stb_image.h>

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
    float phi   = (u - 0.5f) * 2.0f * glm::pi<float>();
    float theta = v * glm::pi<float>();
    glm::vec3 toSun = glm::vec3(glm::sin(theta) * glm::cos(phi),
                                glm::cos(theta),
                                glm::sin(theta) * glm::sin(phi));
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
    , hdrFrag(ShaderGL::FRAGMENT, "shaders/hdr.frag")
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
    BakeCubemap();
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

    // Sky background (equirect for now; Part 1 swaps this to the baked cubemap)
    static constexpr GLuint U_VIEW_DIR   = 0;
    static constexpr GLuint U_VIEW_UP    = 1;
    static constexpr GLuint U_VIEW_RIGHT = 2;
    static constexpr GLuint U_FOV        = 3;
    static constexpr GLuint U_NEAR       = 4;
    static constexpr GLuint T_HDR        = 0;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, ppVert.shaderId);
    glActiveShaderProgram(state.renderPipeline, ppVert.shaderId);
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, hdrFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, hdrFrag.shaderId);
    {
        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_2D, hdrEquirect.textureId);

        glm::vec3 look  = glm::normalize(state.cam.gaze - state.cam.pos);
        glm::vec3 up    = state.cam.up;
        glm::vec3 right = glm::normalize(glm::cross(look, up));
        up   = glm::normalize(glm::cross(right, look));
        look = glm::normalize(glm::cross(up, right));

        glUniform3fv(U_VIEW_DIR, 1, glm::value_ptr(look));
        glUniform3fv(U_VIEW_UP, 1, glm::value_ptr(up));
        glUniform3fv(U_VIEW_RIGHT, 1, glm::value_ptr(right));
        glUniform2fv(U_FOV, 1, glm::value_ptr(fov));
        glUniform1f(U_NEAR, nearFar[0]);
    }
    glBindVertexArray(ppVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Scene
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    terrain.Render(state, hdrEquirect, sun, view, proj, water.waterElevation.y);
    water.Render(state, hdrEquirect, sun, deltaT, view, proj);
    plane.Render(state, hdrEquirect, sun, view, proj);

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
