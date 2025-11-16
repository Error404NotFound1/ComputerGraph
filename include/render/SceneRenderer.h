#pragma once

#include "math/Camera.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace cg
{
    struct GpuMesh
    {
        unsigned vao{0};
        unsigned vbo{0};
        unsigned ebo{0};
        size_t indexCount{0};
        glm::mat4 transform{1.0f};
        unsigned texture{0};
        bool textured{false};
			std::string name;
    };

    struct EnvironmentSettings
    {
        glm::vec3 sunDirection{-0.4f, -1.0f, -0.6f};
        glm::vec3 sunColor{1.0f, 0.96f, 0.85f};
        glm::vec3 ambientSky{0.45f, 0.55f, 0.7f};
        glm::vec3 ambientGround{0.15f, 0.12f, 0.1f};
        glm::vec3 fogColor{0.32f, 0.38f, 0.48f};
        float fogNear{2000.0f};
        float fogFar{25000.0f};
    };

    class SceneRenderer
    {
    public:
        SceneRenderer(const Scene& scene);
        ~SceneRenderer();

        SceneRenderer(const SceneRenderer&) = delete;
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        void draw(const Camera& camera, float aspectRatio);
        void setEnvironmentBlend(float blend);
			bool setMeshTransformByName(const std::string& name, const glm::mat4& transform);

    private:
        std::vector<GpuMesh> m_meshes;
        std::unique_ptr<Shader> m_shader;
        EnvironmentSettings m_dayEnvironment{};
        EnvironmentSettings m_nightEnvironment{};
        float m_environmentBlend{0.0f};
        std::unordered_map<std::string, unsigned> m_textureCache;

        void buildFromScene(const Scene& scene);
        unsigned loadTexture(const std::string& path);
    };
} // namespace cg

