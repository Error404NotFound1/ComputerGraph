#include "render/SceneRenderer.h"

#include <glad/glad.h>

#include "util/Log.h"

#include <cstddef>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <string>
#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <cmath>
#include <cctype>

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
            .sunColor = glm::vec3(1.2f, 1.15f, 1.0f),  // 增强太阳光强度
            .ambientSky = glm::vec3(0.7f, 0.75f, 0.85f),  // 增强天空环境光
            .ambientGround = glm::vec3(0.4f, 0.35f, 0.3f),  // 增强地面环境光
            .fogColor = glm::vec3(0.32f, 0.38f, 0.48f),
            .fogNear = 2000.0f,
            .fogFar = 25000.0f
        };

        m_nightEnvironment = {
            .sunDirection = glm::vec3(-0.2f, -1.0f, -0.2f),
            .sunColor = glm::vec3(0.3f, 0.35f, 0.45f),  // 增强夜间太阳光
            .ambientSky = glm::vec3(0.15f, 0.18f, 0.25f),  // 增强夜间天空环境光
            .ambientGround = glm::vec3(0.08f, 0.08f, 0.12f),  // 增强夜间地面环境光
            .fogColor = glm::vec3(0.05f, 0.07f, 0.12f),
            .fogNear = 1500.0f,
            .fogFar = 20000.0f
        };
        
        // Initialize texture anisotropy level to maximum for best quality
        m_textureAnisotropyLevel = 16.0f;
        
        // Initialize distance-based quality parameters
        m_textureQualityNearDistance = 5000.0f;
        m_textureQualityFarDistance = 25000.0f;
        m_textureQualityMinFactor = 0.3f;
    }

    void SceneRenderer::setTextureAnisotropyLevel(float level)
    {
        m_textureAnisotropyLevel = glm::clamp(level, 1.0f, 16.0f);
        // Note: This only affects newly loaded textures
        // Existing textures keep their original settings
    }

    void SceneRenderer::setTextureQualityDistances(float nearDist, float farDist, float minFactor)
    {
        m_textureQualityNearDistance = glm::max(0.0f, nearDist);
        m_textureQualityFarDistance = glm::max(m_textureQualityNearDistance, farDist);
        m_textureQualityMinFactor = glm::clamp(minFactor, 0.0f, 1.0f);
    }

    void SceneRenderer::setAdvancedMaterialToggles(const MaterialFeatureToggles& toggles)
    {
        m_materialToggles = toggles;
    }

    void SceneRenderer::setEnvironmentMaps(unsigned dayTexture, unsigned nightTexture)
    {
        m_environmentMapDay = dayTexture;
        m_environmentMapNight = nightTexture;
    }

    void SceneRenderer::setLanternLights(const std::vector<LanternLight>& lights)
    {
        m_lanternLights = lights;
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
        // Enable depth testing for proper occlusion
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        
        // Back-face culling will be set per-mesh to handle different winding orders
        
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
        m_shader->setFloat("uEnvironmentBlend", blend);
        
        // Set texture quality parameters for distance-based adjustment
        // These are set per draw call to allow runtime modification via config
        m_shader->setFloat("uTextureQualityNearDistance", m_textureQualityNearDistance);
        m_shader->setFloat("uTextureQualityFarDistance", m_textureQualityFarDistance);
        m_shader->setFloat("uTextureQualityMinFactor", m_textureQualityMinFactor);

        const bool envAvailable = hasEnvironmentMaps();
        m_shader->setInt("uHasEnvironmentMap", envAvailable ? 1 : 0);
        if (envAvailable)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_environmentMapDay);
            m_shader->setInt("uEnvironmentDay", 1);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_environmentMapNight);
            m_shader->setInt("uEnvironmentNight", 2);
            glActiveTexture(GL_TEXTURE0);
        }

        constexpr int kMaxLanternLights = 32;
        int lightCount = static_cast<int>(m_lanternLights.size());
        if (lightCount > kMaxLanternLights)
        {
            lightCount = kMaxLanternLights;
        }
        m_shader->setInt("uLanternLightCount", lightCount);
        for (int i = 0; i < lightCount; ++i)
        {
            const auto& light = m_lanternLights[i];
            const std::string index = std::to_string(i);
            m_shader->setVec3("uLanternLightPos[" + index + "]", light.position);
            m_shader->setVec3("uLanternLightColor[" + index + "]", light.color);
            m_shader->setFloat("uLanternLightIntensity[" + index + "]", light.intensity);
            m_shader->setFloat("uLanternLightRadius[" + index + "]", light.radius);
        }

        // Render all meshes (disable culling to render all faces)
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);  // Disable culling to render all faces
        
        for (const auto& mesh : m_meshes)
        {
            m_shader->setInt("uMaterialMode", determineMaterialMode(mesh));
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
        if (envAvailable)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
        }
    }

    void SceneRenderer::setEnvironmentBlend(float blend)
    {
        m_environmentBlend = blend;
    }

    void SceneRenderer::buildFromScene(const Scene& scene)
    {
        // First pass: collect all unique texture paths and create GPU meshes
        std::unordered_map<std::string, size_t> texturePathToIndex;  // Map texture path to unique index
        std::vector<std::string> texturePaths;  // Unique texture paths
        std::vector<size_t> textureIndices;  // Map mesh index to texture path index
        
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
                // Check if texture is already in cache
                auto cacheIt = m_textureCache.find(mesh.diffuseTexture);
                if (cacheIt != m_textureCache.end())
                {
                    gpuMesh.texture = cacheIt->second;
                    gpuMesh.textured = true;
                    textureIndices.push_back(SIZE_MAX);  // Already loaded, no need to track
                }
                else
                {
                    // Check if this texture path is already in our unique list
                    auto pathIt = texturePathToIndex.find(mesh.diffuseTexture);
                    if (pathIt != texturePathToIndex.end())
                    {
                        // Texture path already added, reuse the index
                        textureIndices.push_back(pathIt->second);
                    }
                    else
                    {
                        // New unique texture path, add it
                        size_t newIndex = texturePaths.size();
                        texturePaths.push_back(mesh.diffuseTexture);
                        texturePathToIndex[mesh.diffuseTexture] = newIndex;
                        textureIndices.push_back(newIndex);
                    }
                    gpuMesh.textured = true;
                }
            }
            else
            {
                textureIndices.push_back(SIZE_MAX);  // No texture
                gpuMesh.textured = false;
            }

            glBindVertexArray(0);

            m_meshes.push_back(gpuMesh);
        }
        
        // Second pass: load textures in parallel (only unique textures)
        if (!texturePaths.empty())
        {
            log(LogLevel::Info, "Starting parallel loading of " + std::to_string(texturePaths.size()) + " unique texture files...");
            std::chrono::high_resolution_clock::time_point loadStart = std::chrono::high_resolution_clock::now();
            loadTexturesParallel(texturePaths);
            std::chrono::high_resolution_clock::time_point loadEnd = std::chrono::high_resolution_clock::now();
            long long loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count();
            log(LogLevel::Info, "Texture loading completed, time: " + std::to_string(loadTime) + "ms");
            
            // Third pass: assign loaded textures to meshes
            for (size_t i = 0; i < m_meshes.size() && i < textureIndices.size(); ++i)
            {
                if (textureIndices[i] != SIZE_MAX)
                {
                    const std::string& texPath = texturePaths[textureIndices[i]];
                    auto it = m_textureCache.find(texPath);
                    if (it != m_textureCache.end())
                    {
                        m_meshes[i].texture = it->second;
                    }
                }
            }
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

    bool SceneRenderer::updateMeshVerticesByName(const std::string& name, const std::vector<Vertex>& vertices)
    {
        for (auto& m : m_meshes)
        {
            if (m.name == name)
            {
                // Update vertex buffer
                glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
                glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
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
        
        // Use SRGB format for better color accuracy and gamma correction
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        // Set texture parameters BEFORE generating mipmaps for optimal quality
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // Enhanced filtering to reduce moiré patterns at distance
        // Use trilinear filtering with high-quality mipmap selection
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Enable anisotropic filtering for better quality at oblique angles and distances
        constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;
        constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY = 0x84FE;
        
        // Enable maximum anisotropic filtering for best distant object quality
        float maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        if (maxAniso > 0.0f)
        {
            const float anisoLevel = glm::clamp(m_textureAnisotropyLevel, 1.0f, maxAniso);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, anisoLevel);
        }
        
        // Generate mipmaps AFTER setting all parameters for best quality
        glGenerateMipmap(GL_TEXTURE_2D);
        
        // Positive LOD bias slightly blurs distant mips to combat shimmering
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0.25f);
        
        // Ensure mipmap levels are explicitly defined for consistent sampling
        int maxDim = std::max(width, height);
        int maxMipLevel = static_cast<int>(std::floor(std::log2(static_cast<float>(maxDim))));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxMipLevel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0.0f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, static_cast<float>(maxMipLevel));
        
        // Enable seamless cubemap filtering if using cubemaps (not applicable here)
        // For 2D textures, ensure proper edge filtering
        // GL_CLAMP_TO_EDGE is already set above for wrap modes
        
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);
        m_textureCache[path] = tex;
        return tex;
    }

    void SceneRenderer::loadTexturesParallel(const std::vector<std::string>& texturePaths)
    {
        if (texturePaths.empty())
        {
            return;
        }
        
        // Filter out already loaded textures
        std::vector<std::string> pathsToLoad;
        for (const auto& path : texturePaths)
        {
            if (m_textureCache.find(path) == m_textureCache.end())
            {
                pathsToLoad.push_back(path);
            }
        }
        
        if (pathsToLoad.empty())
        {
            return;
        }
        
        // Determine number of threads (use hardware concurrency, but limit to reasonable number)
        const size_t numThreads = std::min(pathsToLoad.size(), 
            static_cast<size_t>(std::max(1u, std::thread::hardware_concurrency())));
        
        log(LogLevel::Info, "Using " + std::to_string(numThreads) + " threads to load textures in parallel");
        
        // Split texture paths into chunks for each thread
        const size_t chunkSize = (pathsToLoad.size() + numThreads - 1) / numThreads;
        std::vector<std::vector<std::string>> chunks;
        for (size_t i = 0; i < pathsToLoad.size(); i += chunkSize)
        {
            chunks.push_back(std::vector<std::string>(
                pathsToLoad.begin() + i,
                pathsToLoad.begin() + std::min(i + chunkSize, pathsToLoad.size())));
        }
        
        // Structure to hold loaded texture data
        struct TextureData
        {
            std::string path;
            int width;
            int height;
            int channels;
            stbi_uc* data;
        };
        
        // Load texture data in parallel (I/O operations)
        std::mutex dataMutex;
        std::vector<TextureData> loadedTextures;
        std::vector<std::future<void>> futures;
        
        for (const auto& chunk : chunks)
        {
            futures.push_back(std::async(std::launch::async, [&chunk, &dataMutex, &loadedTextures]() {
                for (const auto& path : chunk)
                {
                    // Load texture data (I/O operation, can be done in parallel)
                    int width = 0;
                    int height = 0;
                    int channels = 0;
                    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
                    
                    if (data)
                    {
                        std::lock_guard<std::mutex> lock(dataMutex);
                        loadedTextures.push_back({path, width, height, channels, data});
                    }
                    else
                    {
                        log(LogLevel::Warn, "Failed to load texture: " + path);
                    }
                }
            }));
        }
        
        // Wait for all threads to complete loading
        for (auto& future : futures)
        {
            future.wait();
        }
        
        // Upload textures to GPU on main thread (OpenGL operations must be on main thread)
        for (const auto& texData : loadedTextures)
        {
            // Check again if texture was loaded by another thread
            if (m_textureCache.find(texData.path) == m_textureCache.end())
            {
                GLuint tex = 0;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                
                // Use SRGB format for better color accuracy
                glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texData.width, texData.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data);
                
                // Enhanced filtering to reduce moiré patterns
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                
                // Use high-quality filtering with better mipmap selection
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                
                // Enhanced anisotropic filtering
                constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY = 0x84FF;
                constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY = 0x84FE;
                
                float maxAniso = 0.0f;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
                if (maxAniso > 0.0f)
                {
                    const float anisoLevel = glm::clamp(m_textureAnisotropyLevel, 1.0f, maxAniso);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, anisoLevel);
                }
                
                // Generate mipmaps with high quality
                glGenerateMipmap(GL_TEXTURE_2D);
                
                // Set LOD bias to reduce moiré at distance (slight negative bias for sharper distant textures)
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.5f);
                
                glBindTexture(GL_TEXTURE_2D, 0);
                
                m_textureCache[texData.path] = tex;
            }
            
            stbi_image_free(texData.data);
        }
    }

    int SceneRenderer::determineMaterialMode(const GpuMesh& mesh) const
    {
        if (mesh.name.empty())
        {
            return 0;
        }

        std::string lower = mesh.name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (m_materialToggles.flagpoleMetal &&
            (lower.find("flagpole_pole") != std::string::npos || lower.find("flagpole_ball") != std::string::npos))
        {
            return 1;
        }

        if (m_materialToggles.missileMetal && lower.find("missile") != std::string::npos)
        {
            return 1;
        }

        if (m_materialToggles.groundTriplanar && isGroundMesh(mesh))
        {
            return 2;
        }

        if (m_materialToggles.flagAnisotropic && lower == "flag")
        {
            return 3;
        }

        // Lanterns: strong emissive glow like a small sun
        if (lower.find("lantern") != std::string::npos)
        {
            return 4;
        }

        return 0;
    }

    bool SceneRenderer::isGroundMesh(const GpuMesh& mesh) const
    {
        if (mesh.name.empty())
        {
            return false;
        }

        std::string lower = mesh.name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return lower.find("ground") != std::string::npos ||
               lower.find("fragment") != std::string::npos ||
               lower.find("slab") != std::string::npos ||
               lower.find("tile") != std::string::npos;
    }
} // namespace cg

