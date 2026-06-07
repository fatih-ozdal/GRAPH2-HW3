#include "hw2.h"
#include <bit>

//
MeshGL::MeshGL(const PlaneGenParams& params)
{
    using namespace glm;
    std::vector<vec3> vertices;
    std::vector<vec3> normals;
    std::vector<unsigned int> indices;
    vertices.reserve(params.vertexCount[1] * params.vertexCount[0]);
    normals.reserve(params.vertexCount[1] * params.vertexCount[0]);
    indices.reserve((params.vertexCount[1] -1) * (params.vertexCount[0] - 1) * 6);

    vec2 delta = vec2(params.rangeX[1] - params.rangeX[0],
                      params.rangeZ[1] - params.rangeZ[0]);
    delta =  delta / (vec2(params.vertexCount) - vec2(1));

    for(uint32_t j = 0; j < params.vertexCount[1]; j++)
    for(uint32_t i = 0; i < params.vertexCount[0]; i++)
    {
        vec2 xz = vec2(params.rangeX[0], params.rangeZ[0]);
        xz += vec2(i, j) * delta;
        vertices.push_back(vec3(xz[0], 0.0f, xz[1]));
        normals.push_back(vec3(0, 1, 0));
    }

    // Indices
    for(uint32_t j = 0; j < (params.vertexCount[1] - 1); j++)
    for(uint32_t i = 0; i < (params.vertexCount[0] - 1); i++)
    {
        // 0,     1, n + 1
        // 0, n + 1, n
        uint32_t ii = j * params.vertexCount[0] + i;
        uint32_t next = ii + params.vertexCount[0];
        indices.push_back(ii      );
        indices.push_back(next + 1);
        indices.push_back(ii   + 1);
        //
        indices.push_back(ii      );
        indices.push_back(next    );
        indices.push_back(next + 1);


    }
    // ===================== //
    //   GEN BUFFER AND VAO  //
    // ===================== //
    // Gen buffer
    std::array<size_t, 3> sizes = {};
    sizes[0] = vertices.size() * sizeof(glm::vec3);
    sizes[1] = normals.size() * sizeof(glm::vec3);
    //
    std::array<size_t, 4> offsets = {};
    offsets[0] = 0;
    for(uint32_t i = 1; i < 4; i++)
    {
        // This may not be necessary, but align the data to 256-byte boundaries
        size_t alignedSize = (sizes[i - 1] + 255) / 256 * 256;
        offsets[i] = offsets[i - 1] + alignedSize;
    }
    glGenBuffers(1, &vBufferId);
    glBindBuffer(GL_ARRAY_BUFFER, vBufferId);
    glBufferStorage(GL_ARRAY_BUFFER, GLintptr(offsets.back()), nullptr, GL_DYNAMIC_STORAGE_BIT);
    // Load the data
    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(offsets[0]), GLsizei(sizes[0]), vertices.data());
    glBufferSubData(GL_ARRAY_BUFFER, GLintptr(offsets[1]), GLsizei(sizes[1]), normals.data());
    // Indices
    glGenBuffers(1, &iBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBufferId);
    glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, GLsizei(indices.size() * sizeof(uint32_t)),
                    indices.data(), GL_DYNAMIC_STORAGE_BIT);

    // VAO
    glGenVertexArrays(1, &vaoId);
    glBindVertexArray(vaoId);
    // Pos (tightly packed vec3)
    glBindVertexBuffer(0, vBufferId, GLintptr(offsets[0]), GLsizei(sizeof(glm::vec3)));
    glEnableVertexAttribArray(0);
    glVertexAttribFormat(0, 3, GL_FLOAT, false, 0);
    // Normal (tightly packed vec3)
    glBindVertexBuffer(1, vBufferId, GLintptr(offsets[1]), GLsizei(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);
    glVertexAttribFormat(1, 3, GL_FLOAT, false, 0);
    //
    glVertexAttribBinding(0, IN_POS);
    glVertexAttribBinding(1, IN_NORMAL);
    // Above API calls are understandable but to use index draw calls
    // we need to bind an element array buffer (aka. index buffer)
    // to make the vao to store indices so that we can call draw elements call
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iBufferId);
    glBindVertexArray(0);

    indexCount = uint32_t(indices.size());
    assert(indexCount % 3 == 0);

    glBindVertexArray(0);
}

Plane::Plane()
    : vert(ShaderGL::VERTEX, "shaders/plane.vert")
    , frag(ShaderGL::FRAGMENT, "shaders/plane.frag")
    , bodyMesh("meshes/plane_body.obj")
    , helixMesh("meshes/plane_helix.obj")
    , glassMesh("meshes/plane_glass.obj")
    , cablesMesh("meshes/plane_cable.obj")
    , velocity(0.0f)
    , bodyAlbedoTex("textures/plane_base_albedo.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , bodyRoughnessTex("textures/plane_base_roughness.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , helixAlbedoTex("textures/plane_helix_albedo.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , helixRoughnessTex("textures/plane_helix_roughness.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
{
}

void Plane::Update(const GLState& state, float deltaT)
{
    glm::vec3 look = glm::normalize(glm::rotate(rotation, glm::vec3(0, 0, 1)));
    position += look * deltaT * velocity;
    propRotation += deltaT * 60.5f;
}

void Plane::Render(const GLState& state, const TextureGL& hdr,
                   const SunParams& sun,
                   const glm::mat4& view, const glm::mat4& proj)
{
    static constexpr GLuint U_TRANSFORM_MODEL  = 0;
    static constexpr GLuint U_TRANSFORM_VIEW   = 1;
    static constexpr GLuint U_TRANSFORM_PROJ   = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL = 3;
    //
    static constexpr GLuint U_MODE = 0;
    static constexpr GLuint U_SUN_DIR = 1;
    static constexpr GLuint U_SUN_POW = 2;
    static constexpr GLuint U_CAM_POS = 3;
    //
    static constexpr GLuint T_BODY_ALBEDO	= 0;
    static constexpr GLuint T_BODY_ROUGH	= 1;
    static constexpr GLuint T_HELIX_ALBEDO	= 2;
    static constexpr GLuint T_HELIX_ROUGH	= 3;
    static constexpr GLuint T_HDR			= 4;

    // Common
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));

    }
    // Fragment shader
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));

        static auto SetTex = [](GLuint index, const TextureGL& tex)
        {
            glActiveTexture(GL_TEXTURE0 + index);
            glBindTexture(GL_TEXTURE_2D, tex.textureId);
        };

        SetTex(T_BODY_ALBEDO, bodyAlbedoTex);
        SetTex(T_BODY_ROUGH, bodyRoughnessTex);
        SetTex(T_HELIX_ALBEDO, helixAlbedoTex);
        SetTex(T_HELIX_ROUGH, helixRoughnessTex);
        SetTex(T_HDR, hdr);
    }

    enum RenderMode : unsigned int
    {
        BODY  = 0,
        HELIX = 1,
        CABLE = 2,
        GLASS = 3
    };

    // Base model
    glm::mat4x4 r = glm::toMat4(rotation);
    glm::mat4x4 t = glm::translate(glm::identity<glm::mat4x4>(), position);
    glm::mat4x4 s = glm::scale(glm::identity<glm::mat4x4>(), glm::vec3(0.3f));
    glm::mat4x4 model = t * s * r;

    // ============ //
    //     BODY     //
    // ============ //
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        // Normal local->world matrix of the object
        glm::mat4x4 bodyModel = model;
        glm::mat3x3 normalMatrix = glm::inverseTranspose(bodyModel);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(bodyModel));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
    }
    //
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform1ui(U_MODE, BODY);
    }
    glBindVertexArray(bodyMesh.vaoId);
    glDrawElements(GL_TRIANGLES, bodyMesh.indexCount, GL_UNSIGNED_INT, nullptr);
    // ============ //
    //    HELIX     //
    // ============ //
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        glm::mat4x4 localRot = glm::rotate(glm::identity<glm::mat4x4>(), propRotation, glm::vec3(0, 0, 1));
        glm::mat4x4 helixModel = glm::translate(localRot, localHelixTranslate);
        helixModel = model * helixModel;
        glm::mat3x3 normalMatrix = glm::inverseTranspose(helixModel);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(helixModel));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
    }
    //
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform1ui(U_MODE, HELIX);
    }
    glBindVertexArray(helixMesh.vaoId);
    glDrawElements(GL_TRIANGLES, helixMesh.indexCount, GL_UNSIGNED_INT, nullptr);
    // ============ //
    //    CABLE     //
    // ============ //
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        glm::mat4x4 cableModel = glm::translate(glm::identity<glm::mat4x4>(), localCableTranslate);
        cableModel = model * cableModel;
        glm::mat3x3 normalMatrix = glm::inverseTranspose(cableModel);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(cableModel));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
    }
    //
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform1ui(U_MODE, CABLE);
    }
    glBindVertexArray(cablesMesh.vaoId);
    glDrawElements(GL_TRIANGLES, cablesMesh.indexCount, GL_UNSIGNED_INT, nullptr);
    // ============ //
    //    GLASS     //
    // ============ //
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        glm::mat4x4 glassModel = glm::translate(glm::identity<glm::mat4x4>(), localGlassTranslate);
        glassModel = model * glassModel;
        glm::mat3x3 normalMatrix = glm::inverseTranspose(glassModel);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(glassModel));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
    }
    //
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform1ui(U_MODE, GLASS);
    }
    glBindVertexArray(glassMesh.vaoId);
    glDrawElements(GL_TRIANGLES, glassMesh.indexCount, GL_UNSIGNED_INT, nullptr);
}

void Plane::ChangeYaw(float delta)
{
    glm::quat yaw = glm::angleAxis(delta, glm::vec3(0, 1, 0));
    //rotation = yaw * rotation;
    rotation = rotation * yaw;
    rotation = glm::normalize(rotation);
}
void Plane::ChangePitch(float delta)
{
    glm::quat pitch = glm::angleAxis(delta, glm::vec3(1, 0, 0));
    //rotation = pitch * rotation;
    rotation = rotation * pitch;
    rotation = glm::normalize(rotation);
}

void Plane::ChangeRoll(float delta)
{
    glm::quat roll = glm::angleAxis(delta, glm::vec3(0, 0, 1));
    //rotation = roll * rotation;
    rotation = rotation * roll;
    rotation = glm::normalize(rotation);
}

void Plane::ChangeVelocity(float deltaV)
{
    velocity += deltaV;
}

Water::Water(float waterElevation,
             const PlaneGenParams& planeParams)
    : vert(ShaderGL::VERTEX, "shaders/water.vert")
    , frag(ShaderGL::FRAGMENT, "shaders/water.frag")
    , waterColor(0.05f, 0.09f, 0.6f)
    , waterElevation(waterElevation)
    , mesh(planeParams)
{

}

void Water::Render(const GLState& state, const TextureGL& hdr,
                   const SunParams& sun, float deltaT,
                   const glm::mat4& view, const glm::mat4& proj)
{
    static constexpr GLuint U_TRANSFORM_MODEL     = 0;
    static constexpr GLuint U_TRANSFORM_VIEW      = 1;
    static constexpr GLuint U_TRANSFORM_PROJ      = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL    = 3;
    static constexpr GLuint U_TIME                = 4;
    //
    static constexpr GLuint U_SUN_DIR     = 0;
    static constexpr GLuint U_SUN_POW     = 1;
    static constexpr GLuint U_WATER_COLOR = 2;
    static constexpr GLuint U_CAM_POS     = 3;
    static constexpr GLuint U_WATER_IOR   = 4;

    static constexpr GLuint T_HDR = 0;

    timeElapsed += deltaT;

    // glActiveShaderProgram makes "glUniform" family of commands
    // to effect the selected shader
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        // Don't move the triangle
        glm::mat4x4 model = glm::translate(glm::identity<glm::mat4x4>(), waterElevation);
        // Normal local->world matrix of the object
        glm::mat3x3 normalMatrix = glm::inverseTranspose(model);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(model));
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
        glUniform1f(U_TIME, timeElapsed);
    }
    // Fragment shader
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform3fv(U_WATER_COLOR, 1, glm::value_ptr(waterColor));
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));
        glUniform1f(U_WATER_IOR, waterIoR);

        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_2D, hdr.textureId);
    }
    // Bind VAO
    glBindVertexArray(mesh.vaoId);
    // Draw call!
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
}

Terrain_v2::Terrain_v2(ThreadPool& threadPool,
                       const TerrainMeshGenerationParams& terrainParams)
    : vert(ShaderGL::VERTEX, "shaders/terrain_v2.vert")
    , frag(ShaderGL::FRAGMENT, "shaders/terrain_v2.frag")
    , snowAlbedo("textures/snow_diff_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , snowRoughness("textures/snow_rough_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , rockAlbedo("textures/rock_diff_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , rockRoughness("textures/rock_rough_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , shoreAlbedo("textures/shore_diff_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , shoreRoughness("textures/shore_rough_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , grassAlbedo("textures/grass_diff_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
    , grassRoughness("textures/grass_rough_2k.jpg", TextureGL::LINEAR, TextureGL::REPEAT)
{
    terrain = GenerateTerrainBSpline(threadPool,
                                     GeoDataDTED("geo/n36_e029_1arc_v3.dt2"),
                                     terrainParams);
}

void Terrain_v2::Render(const GLState& state,
                        const TextureGL& hdr,
                        const SunParams& sun,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        float waterLevel)
{
    // Bind shaders and related uniforms / textures
    // Move this somewhere proper later.
    // These must match the uniform "location" at the shader(s).
    // Vertex
    static constexpr GLuint U_TRANSFORM_MODEL     = 0;
    static constexpr GLuint U_TRANSFORM_VIEW      = 1;
    static constexpr GLuint U_TRANSFORM_PROJ      = 2;
    static constexpr GLuint U_TRANSFORM_NORMAL    = 3;
    static constexpr GLuint U_VERTEX_HEIGHT_SCALE = 4;
    // Fragment
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


    // glActiveShaderProgram makes "glUniform" family of commands
    // to effect the selected shader
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, vert.shaderId);
    glActiveShaderProgram(state.renderPipeline, vert.shaderId);
    {
        // Don't move the triangle
        glm::mat4x4 model = glm::identity<glm::mat4x4>();
        // Normal local->world matrix of the object
        glm::mat3x3 normalMatrix = glm::inverseTranspose(model);
        glUniformMatrix4fv(U_TRANSFORM_MODEL, 1, false, glm::value_ptr(model));
        glUniformMatrix4fv(U_TRANSFORM_VIEW, 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(U_TRANSFORM_PROJ, 1, false, glm::value_ptr(proj));
        glUniformMatrix3fv(U_TRANSFORM_NORMAL, 1, false, glm::value_ptr(normalMatrix));
        glUniform1f(U_VERTEX_HEIGHT_SCALE, 1.0f);
    }
    // Fragment shader
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, frag.shaderId);
    glActiveShaderProgram(state.renderPipeline, frag.shaderId);
    {
        glUniform2fv(U_RANGE, 1, glm::value_ptr(terrain.yMinMax));
        glUniform3fv(U_SUN_DIR, 1, glm::value_ptr(sun.direction));
        glUniform1f(U_SUN_POW, sun.power);
        glUniform1f(U_WATER_LEVEL, waterLevel);
        glUniform3fv(U_CAM_POS, 1, glm::value_ptr(state.cam.pos));

        static auto SetTex = [](GLuint index, const TextureGL& tex)
        {
            glActiveTexture(GL_TEXTURE0 + index);
            glBindTexture(GL_TEXTURE_2D, tex.textureId);
        };

        SetTex(T_SNOW_ALBEDO, snowAlbedo);
        SetTex(T_SNOW_ROUGH, snowRoughness);
        SetTex(T_ROCK_ALBEDO, rockAlbedo);
        SetTex(T_ROCK_ROUGH, rockRoughness);
        SetTex(T_SHORE_ALBEDO, shoreAlbedo);
        SetTex(T_SHORE_ROUGH, shoreRoughness);
        SetTex(T_GRASS_ALBEDO, grassAlbedo);
        SetTex(T_GRASS_ROUGH, grassRoughness);
        SetTex(T_HDR, hdr);

    }
    // Bind VAO
    glBindVertexArray(terrain.vaoId);
    // Draw call!
    glDrawElements(GL_TRIANGLES, terrain.indexCount, GL_UNSIGNED_INT, nullptr);
}

HW2::HW2(ThreadPool& threadPool, GLState& state)
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
    //, hdr("textures/qwantani_sunset_puresky_4k.hdr", TextureGL::LINEAR, TextureGL::REPEAT)
    , hdr("textures/citrus_orchard_puresky_2k.hdr", TextureGL::LINEAR, TextureGL::REPEAT)
    //, hdr("textures/church_stairway_2k.hdr", TextureGL::LINEAR, TextureGL::REPEAT)
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
    //
    glVertexAttribBinding(0, 0);
    //
    glBindVertexArray(0);
}

void HW2::ResetFramebuffer()
{
    if(hdrFramebuffer) glDeleteFramebuffers(1, &hdrFramebuffer);
    if(frameColorTex)  glDeleteTextures(1, &frameColorTex);
    if(frameDepthTex)  glDeleteTextures(1, &frameDepthTex);
    hdrFramebuffer = 0;
    frameColorTex = 0;
    frameDepthTex = 0;

    // Create HDR FBO
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
    // Check Status
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::fprintf(stderr, "Incomplete framebuffer!\n");
        std::exit(EXIT_FAILURE);
    }
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void HW2::Work()
{
    // Timing
    static float lastTime = 0;
    float curTime = float(glfwGetTime());
    float deltaT = curTime - lastTime;
    lastTime = curTime;

    //
    plane.ChangePitch(state.pitch); state.pitch = 0.0f;
    plane.ChangeRoll(state.roll); state.roll = 0.0f;
    plane.ChangeYaw(state.yaw); state.yaw = 0.0f;
    plane.ChangeVelocity(state.speedModifier); state.speedModifier = 0.0f;
    plane.Update(state, deltaT);

    // Track the plane
    glm::vec3 deltaVec = plane.position - state.cam.gaze;
    state.cam.gaze = plane.position;
    state.cam.pos += deltaVec;

    //
    if(state.curWndParams.fbSize[0] == 0 ||
       state.curWndParams.fbSize[1] == 0)
        return;

    // Object-common matrices
    float aspectRatio = float(state.curWndParams.fbSize[0]) / float(state.curWndParams.fbSize[1]);
    float fovY = glm::radians(50.0f);
    glm::vec2 nearFar (0.1f, 500.0f);
    // Aspect correct Fov
    glm::vec2 fov = glm::vec2(2.0f * std::atan(std::tan(fovY * 0.5f) * aspectRatio),
                              fovY);
    glm::mat4x4 proj = glm::perspective(fovY, aspectRatio, nearFar[0], nearFar[1]);
    glm::mat4x4 view = glm::lookAt(state.cam.pos, state.cam.gaze, state.cam.up);

    if(curFBSize != state.curWndParams.fbSize)
    {
        ResetFramebuffer();
        curFBSize = state.curWndParams.fbSize;
    };

    // Render HDR Map as background
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdrFramebuffer);
    glViewport(0, 0, curFBSize[0], curFBSize[1]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, state.wireframe ? GL_LINE : GL_FILL);

    // HDR WRITE
    static constexpr GLuint U_VIEW_DIR   = 0;
    static constexpr GLuint U_VIEW_UP    = 1;
    static constexpr GLuint U_VIEW_RIGHT = 2;
    static constexpr GLuint U_FOV        = 3;
    static constexpr GLuint U_NEAR       = 4;
    //
    static constexpr GLuint T_HDR       = 0;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, ppVert.shaderId);
    glActiveShaderProgram(state.renderPipeline, ppVert.shaderId);
    {
        // Nothing to set
    }
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, hdrFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, hdrFrag.shaderId);
    {
        glActiveTexture(GL_TEXTURE0 + T_HDR);
        glBindTexture(GL_TEXTURE_2D, hdr.textureId);

        glm::vec3 look = glm::normalize(state.cam.gaze - state.cam.pos);
        glm::vec3 up    = state.cam.up;
        glm::vec3 right = glm::cross(look, up);
        right = glm::normalize(glm::cross(look, up));
        up = glm::normalize(glm::cross(right, look));
        look = glm::normalize(glm::cross(up, right));

        glUniform3fv(U_VIEW_DIR, 1, glm::value_ptr(look));
        glUniform3fv(U_VIEW_UP, 1, glm::value_ptr(up));
        glUniform3fv(U_VIEW_RIGHT, 1, glm::value_ptr(right));
        glUniform2fv(U_FOV, 1, glm::value_ptr(fov));
        glUniform1f(U_NEAR, nearFar[0]);
    }
    glBindVertexArray(ppVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);


    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    terrain.Render(state, hdr, sun, view, proj, water.waterElevation.y);
    water.Render(state, hdr, sun, deltaT, view, proj);
    plane.Render(state, hdr, sun, view, proj);

    // Final Present to the Window
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, curFBSize[0], curFBSize[1]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Generate Mips of the HDR Framebuffer
    glBindTexture(GL_TEXTURE_2D, frameColorTex);
    glGenerateMipmap(GL_TEXTURE_2D);
    // TONE MAP
    static constexpr GLuint U_MID_GRAY  = 0;
    static constexpr GLuint U_PIX_COUNT = 1;
    static constexpr GLuint U_WHITE     = 2;
    //
    static constexpr GLuint T_HDR_FRAME = 0;

    // GUI Params
    static bool hdrIsOpen = true;
    ImGui::Begin("HDR Params", &hdrIsOpen);
    ImGui::SliderFloat("Middle Gray", &middleGray, 0.001f, 1.0f);
    ImGui::SliderFloat("LWhite (x10^6)", &lWhite, 0.01f, 10.0f);
    ImGui::End();

    glUseProgramStages(state.renderPipeline, GL_VERTEX_SHADER_BIT, ppVert.shaderId);
    glActiveShaderProgram(state.renderPipeline, ppVert.shaderId);
    {
        // Nothing to set
    }
    glUseProgramStages(state.renderPipeline, GL_FRAGMENT_SHADER_BIT, toneMapFrag.shaderId);
    glActiveShaderProgram(state.renderPipeline, toneMapFrag.shaderId);
    {
        glActiveTexture(GL_TEXTURE0 + T_HDR_FRAME);
        glBindTexture(GL_TEXTURE_2D, frameColorTex);

        glUniform1f(U_MID_GRAY, middleGray);
        glUniform1ui(U_PIX_COUNT, curFBSize[0] * curFBSize[1]);
        glUniform1f(U_WHITE, lWhite * 1'000'000);

    }
    glBindVertexArray(ppVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

}

HW2::~HW2()
{
    if(hdrFramebuffer) glDeleteFramebuffers(1, &hdrFramebuffer);
    if(frameColorTex) glDeleteTextures(1, &frameColorTex);
    if(frameDepthTex) glDeleteTextures(1, &frameDepthTex);
    //
    hdrFramebuffer = 0;
    frameColorTex = 0;
    frameDepthTex = 0;
    //
    if(ppVertexBuffer) glDeleteBuffers(1, &ppVertexBuffer);
    if(ppVAO)          glDeleteVertexArrays(1, &ppVAO);
    //
    ppVertexBuffer = 0;
    ppVAO = 0;

}