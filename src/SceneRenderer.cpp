#include "render/SceneRenderer.h"

#include <glad/glad.h>

#include "util/Log.h"

#include <cstddef>
#include <stb/stb_image.h>
#include <glm/glm.hpp>

namespace cg
{
    namespace
    {
        constexpr GLuint kPosLocation = 0;
        constexpr GLuint kNormalLocation = 1;
        constexpr GLuint kUvLocation = 2;
        constexpr GLuint kColorLocation = 3;
    }

    SceneRenderer::SceneRenderer(const Scene& scene)
    {
        m_shader = std::make_unique<Shader>("shaders/standard.vert", "shaders/standard.frag");
        buildFromScene(scene);

        m_dayEnvironment = {
            .sunDirection = glm::vec3(-0.4f, -1.0f, -0.6f),
            .sunColor = glm::vec3(1.0f, 0.96f, 0.85f),
            .ambientSky = glm::vec3(0.45f, 0.55f, 0.7f),
            .ambientGround = glm::vec3(0.15f, 0.12f, 0.1f),
            .fogColor = glm::vec3(0.32f, 0.38f, 0.48f),
            .fogNear = 2000.0f,
            .fogFar = 25000.0f
        };

        m_nightEnvironment = {
            .sunDirection = glm::vec3(-0.2f, -1.0f, -0.2f),
            .sunColor = glm::vec3(0.2f, 0.25f, 0.35f),
            .ambientSky = glm::vec3(0.08f, 0.1f, 0.2f),
            .ambientGround = glm::vec3(0.02f, 0.02f, 0.04f),
            .fogColor = glm::vec3(0.05f, 0.07f, 0.12f),
            .fogNear = 1500.0f,
            .fogFar = 20000.0f
        };
    }

    SceneRenderer::~SceneRenderer()
    {
        for (const auto& mesh : m_meshes)
        {
            glDeleteVertexArrays(1, &mesh.vao);
            glDeleteBuffers(1, &mesh.vbo);
            glDeleteBuffers(1, &mesh.ebo);
        }

        for (auto& entry : m_textureCache)
        {
            if (entry.second != 0)
            {
                glDeleteTextures(1, &entry.second);
            }
        }
    }

    void SceneRenderer::draw(const Camera& camera, float aspectRatio)
    {
        const float blend = glm::clamp(m_environmentBlend, 0.0f, 1.0f);
        EnvironmentSettings env{};
        env.sunDirection = glm::normalize(glm::mix(m_dayEnvironment.sunDirection, m_nightEnvironment.sunDirection, blend));
        env.sunColor = glm::mix(m_dayEnvironment.sunColor, m_nightEnvironment.sunColor, blend);
        env.ambientSky = glm::mix(m_dayEnvironment.ambientSky, m_nightEnvironment.ambientSky, blend);
        env.ambientGround = glm::mix(m_dayEnvironment.ambientGround, m_nightEnvironment.ambientGround, blend);
        env.fogColor = glm::mix(m_dayEnvironment.fogColor, m_nightEnvironment.fogColor, blend);
        env.fogNear = glm::mix(m_dayEnvironment.fogNear, m_nightEnvironment.fogNear, blend);
        env.fogFar = glm::mix(m_dayEnvironment.fogFar, m_nightEnvironment.fogFar, blend);

        m_shader->bind();
        m_shader->setMat4("uView", camera.viewMatrix());
        m_shader->setMat4("uProj", camera.projectionMatrix(aspectRatio));
        m_shader->setVec3("uCameraPos", camera.position());
        m_shader->setVec3("uSunDir", glm::normalize(env.sunDirection));
        m_shader->setVec3("uSunColor", env.sunColor);
        m_shader->setVec3("uAmbientSky", env.ambientSky);
        m_shader->setVec3("uAmbientGround", env.ambientGround);
        m_shader->setVec3("uFogColor", env.fogColor);
        m_shader->setFloat("uFogNear", env.fogNear);
        m_shader->setFloat("uFogFar", env.fogFar);

        for (const auto& mesh : m_meshes)
        {
            m_shader->setMat4("uModel", mesh.transform);
            m_shader->setInt("uUseTexture", mesh.textured ? 1 : 0);
            if (mesh.textured)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mesh.texture);
                m_shader->setInt("uDiffuse", 0);
            }
            glBindVertexArray(mesh.vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indexCount), GL_UNSIGNED_INT, nullptr);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
    }

    void SceneRenderer::setEnvironmentBlend(float blend)
    {
        m_environmentBlend = blend;
    }

    void SceneRenderer::buildFromScene(const Scene& scene)
    {
        for (const auto& mesh : scene.meshes())
        {
            GpuMesh gpuMesh{};
            gpuMesh.indexCount = mesh.indices.size();
            gpuMesh.transform = mesh.transform;
            gpuMesh.name = mesh.name;

            glGenVertexArrays(1, &gpuMesh.vao);
            glGenBuffers(1, &gpuMesh.vbo);
            glGenBuffers(1, &gpuMesh.ebo);

            glBindVertexArray(gpuMesh.vao);

            glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.vbo);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(Vertex), mesh.vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpuMesh.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(kPosLocation);
            glVertexAttribPointer(kPosLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

            glEnableVertexAttribArray(kNormalLocation);
            glVertexAttribPointer(kNormalLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

            glEnableVertexAttribArray(kUvLocation);
            glVertexAttribPointer(kUvLocation, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));

            glEnableVertexAttribArray(kColorLocation);
            glVertexAttribPointer(kColorLocation, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

            if (!mesh.diffuseTexture.empty())
            {
                gpuMesh.texture = loadTexture(mesh.diffuseTexture);
                gpuMesh.textured = gpuMesh.texture != 0;
            }

            glBindVertexArray(0);

            m_meshes.push_back(gpuMesh);
        }
    }

    bool SceneRenderer::setMeshTransformByName(const std::string& name, const glm::mat4& transform)
    {
        for (auto& m : m_meshes)
        {
            if (m.name == name)
            {
                m.transform = transform;
                return true;
            }
        }
        return false;
    }

    unsigned SceneRenderer::loadTexture(const std::string& path)
    {
        if (path.empty())
        {
            return 0;
        }

        auto it = m_textureCache.find(path);
        if (it != m_textureCache.end())
        {
            return it->second;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            log(LogLevel::Warn, "Failed to load texture: " + path);
            return 0;
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);
        m_textureCache[path] = tex;
        return tex;
    }
} // namespace cg

