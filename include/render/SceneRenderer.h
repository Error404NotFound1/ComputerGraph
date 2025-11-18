#pragma once

#include "math/Camera.h"
#include "render/Shader.h"
#include "scene/Scene.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>
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

    struct MaterialFeatureToggles
    {
        bool flagpoleMetal{false};
        bool missileMetal{false};
        bool groundTriplanar{false};
        bool flagAnisotropic{false};
    };

    class SceneRenderer
    {
    public:
        struct LanternLight
        {
            glm::vec3 position{0.0f};
            glm::vec3 color{1.0f};
            float intensity{1.0f};
            float radius{1000.0f};
        };
        SceneRenderer(const Scene& scene);
        ~SceneRenderer();

        SceneRenderer(const SceneRenderer&) = delete;
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        void draw(const Camera& camera, float aspectRatio);
        void setEnvironmentBlend(float blend);
        bool setMeshTransformByName(const std::string& name, const glm::mat4& transform);
        bool updateMeshVerticesByName(const std::string& name, const std::vector<Vertex>& vertices);
        void setTextureAnisotropyLevel(float level);  // Set anisotropic filtering level for new textures
        void setTextureQualityDistances(float nearDist, float farDist, float minFactor);  // Set distance-based quality parameters
        void setAdvancedMaterialToggles(const MaterialFeatureToggles& toggles);
        void setEnvironmentMaps(unsigned dayTexture, unsigned nightTexture);
        void setLanternLights(const std::vector<LanternLight>& lights);

    private:
        std::vector<GpuMesh> m_meshes;
        std::unique_ptr<Shader> m_shader;
        EnvironmentSettings m_dayEnvironment{};
        EnvironmentSettings m_nightEnvironment{};
        float m_environmentBlend{0.0f};
        std::unordered_map<std::string, unsigned> m_textureCache;
        float m_textureAnisotropyLevel{16.0f};  // Anisotropic filtering level
        
        // Distance-based texture quality parameters
        float m_textureQualityNearDistance{5000.0f};
        float m_textureQualityFarDistance{25000.0f};
        float m_textureQualityMinFactor{0.3f};

        MaterialFeatureToggles m_materialToggles{};
        unsigned m_environmentMapDay{0};
        unsigned m_environmentMapNight{0};
        std::vector<LanternLight> m_lanternLights;

        void buildFromScene(const Scene& scene);
        unsigned loadTexture(const std::string& path);
        void loadTexturesParallel(const std::vector<std::string>& texturePaths);  // Multi-threaded texture loading
        int determineMaterialMode(const GpuMesh& mesh) const;
        bool isGroundMesh(const GpuMesh& mesh) const;
        bool hasEnvironmentMaps() const { return m_environmentMapDay != 0 && m_environmentMapNight != 0; }
    };
} // namespace cg

