#include "core/App.h"
#include "core/AppConfig.h"

#include "util/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <thread>
#include <future>
#include <mutex>
#include <vector>
#include <chrono>
#include <optional>
#include <random>

namespace
{
    using RandomEngine = std::mt19937;

    RandomEngine& globalRandomEngine()
    {
        thread_local RandomEngine engine(std::random_device{}());
        return engine;
    }

    float randomFloat(float minValue, float maxValue)
    {
        if (minValue > maxValue)
        {
            std::swap(minValue, maxValue);
        }
        std::uniform_real_distribution<float> dist(minValue, maxValue);
        return dist(globalRandomEngine());
    }

    int randomInt(int minValue, int maxValue)
    {
        if (minValue > maxValue)
        {
            std::swap(minValue, maxValue);
        }
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        return dist(globalRandomEngine());
    }

    glm::vec3 randomUnitVector()
    {
        const float z = randomFloat(-1.0f, 1.0f);
        const float theta = randomFloat(0.0f, glm::two_pi<float>());
        const float radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
        return glm::vec3(radius * std::cos(theta), z, radius * std::sin(theta));
    }

    glm::mat4 applyScale(const glm::mat4& matrix, const glm::vec3& scale)
    {
        glm::mat4 result = matrix;
        result[0] *= scale.x;
        result[1] *= scale.y;
        result[2] *= scale.z;
        return result;
    }
}

namespace cg
{
    App::App(AppConfig config)
        : m_config(std::move(config))
    {
    }

    int App::run()
    {
        if (!m_glfwContext.isInitialized())
        {
            log(LogLevel::Error, "Failed to initialize GLFW");
            return EXIT_FAILURE;
        }

        // Configure MSAA before creating window
        m_glfwContext.setMSAASamples(m_config.msaaSamples);
        if (m_config.msaaSamples > 0)
        {
            log(LogLevel::Info, "MSAA enabled with " + std::to_string(m_config.msaaSamples) + " samples");
        }

        try
        {
            m_window = std::make_unique<Window>(m_config.windowWidth, m_config.windowHeight, m_config.windowTitle, m_config.enableVSync);
        }
        catch (const std::runtime_error& err)
        {
            log(LogLevel::Error, err.what());
            return EXIT_FAILURE;
        }

        m_window->setInputState(&m_input);

        // Preload all resources before starting animation
        log(LogLevel::Info, "Starting resource preloading...");
        if (!preloadResources())
        {
            log(LogLevel::Error, "Resource preloading failed!");
            return EXIT_FAILURE;
        }
        log(LogLevel::Info, "Resource preloading completed. Starting animation...");

        // Initialize viewport
        int framebufferWidth = m_config.windowWidth;
        int framebufferHeight = m_config.windowHeight;
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // Reset timer and total time - animation starts now
        m_timer.reset();
        m_totalTime = 0.0;
        m_lastCameraLogTime = 0.0;
        
        double deltaSeconds = 0.0;
        
        while (!m_window->shouldClose())
        {
            deltaSeconds = m_timer.tick();
            m_totalTime += deltaSeconds;
            processInput(deltaSeconds);
            updateSkyBlend(deltaSeconds);
            updateCameraMotion(deltaSeconds);  // Update camera motion before other animations
            updateAirplaneAnimation(deltaSeconds);
            updateMissileAnimation(deltaSeconds);
            updateFlagAnimation(deltaSeconds);
            updateLanterns(deltaSeconds);
            updateParticleEffects(deltaSeconds);
            
            // Log camera info periodically
            if (m_totalTime - m_lastCameraLogTime >= CAMERA_LOG_INTERVAL)
            {
                const glm::vec3 camPos = m_camera.position();
                const float yaw = m_camera.yaw();
                const float pitch = m_camera.pitch();
                
                std::ostringstream logMsg;
                logMsg << std::fixed << std::setprecision(2) 
                       << "Time: " << m_totalTime << "s | "
                       << "Position: (" << std::setprecision(1) << camPos.x << ", " << camPos.y << ", " << camPos.z << ") | "
                       << "Rotation: Yaw=" << yaw << " Pitch=" << pitch;
                
                log(LogLevel::Info, logMsg.str());
                m_lastCameraLogTime = m_totalTime;
            }
            m_window->pollEvents();
            glfwGetFramebufferSize(m_window->rawHandle(), &framebufferWidth, &framebufferHeight);

            // Ensure viewport matches framebuffer size (handle 0 or negative sizes)
            if (framebufferWidth > 0 && framebufferHeight > 0)
            {
                glViewport(0, 0, framebufferWidth, framebufferHeight);
            }

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);  // Ensure depth test is using LESS (standard)
            
            // Disable face culling to render all faces (some models may have incorrect winding order)
            glDisable(GL_CULL_FACE);
            
            // Ensure proper depth buffer precision
            glDepthRange(0.0, 1.0);
            
            glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = framebufferHeight > 0 ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight) : 1.0f;
            m_skybox->draw(m_camera, aspect, m_skyBlend, m_config.daySkyboxYOffset, m_config.nightSkyboxYOffset);
            m_renderer->setEnvironmentBlend(m_skyBlend);
            m_renderer->draw(m_camera, aspect);
            if (m_particleSystem)
            {
                m_particleSystem->draw(m_camera, aspect);
            }

            // Draw time and camera info on screen (top right corner) if enabled
            if (m_config.showTimeDisplay)
            {
                // Position text from right edge, but ensure it's visible
                // Use a smaller offset to keep text more to the left
                // Ensure text is always visible by using a reasonable offset
                // Use smaller offset (150px) and ensure minimum left margin of 20px
                const float textX = std::max(20.0f, static_cast<float>(framebufferWidth) - 150.0f); // Right side, but ensure minimum left margin
                float textY = 30.0f; // Top margin
                
                // Display time
                std::ostringstream timeStr;
                timeStr << std::fixed << std::setprecision(2) << "Time: " << m_totalTime << "s";
                m_textRenderer->drawText(timeStr.str(), textX, textY, m_config.timeDisplayScale, glm::vec3(1.0f, 1.0f, 0.0f)); // Yellow color
                textY += 35.0f; // Move down for next line
                
                // Display camera position
                const glm::vec3 camPos = m_camera.position();
                std::ostringstream posStr;
                posStr << std::fixed << std::setprecision(1) 
                       << "Pos: (" << camPos.x << ", " << camPos.y << ", " << camPos.z << ")";
                m_textRenderer->drawText(posStr.str(), textX, textY, m_config.timeDisplayScale, glm::vec3(0.8f, 0.8f, 1.0f)); // Light blue color
                textY += 35.0f; // Move down for next line
                
                // Display camera rotation (yaw and pitch)
                std::ostringstream rotStr;
                rotStr << std::fixed << std::setprecision(1) 
                       << "Rot: Yaw=" << m_camera.yaw() << " Pitch=" << m_camera.pitch();
                m_textRenderer->drawText(rotStr.str(), textX, textY, m_config.timeDisplayScale, glm::vec3(0.8f, 1.0f, 0.8f)); // Light green color
            }

            m_window->swapBuffers();
        }

        return EXIT_SUCCESS;
    }

    bool App::preloadResources()
    {
        // Build base scene meshes (ground tiles, etc.)
        log(LogLevel::Info, "Loading ground meshes...");
        auto meshes = buildDemoScene(m_config.groundMeshPath, m_config.groundTilesPerSide);

        // Collect all model paths that need to be loaded
        struct ModelLoadTask
        {
            std::string path;
            std::string name;
            bool isMeshes;  // true for loadObjAsMeshes, false for loadObjAsMesh
        };
        
        std::vector<ModelLoadTask> loadTasks;
        
        if (!m_config.airplaneModelPath.empty())
        {
            loadTasks.push_back({m_config.airplaneModelPath, "airplane", false});
        }
        
        if (!m_config.wingmanModelPath.empty())
        {
            // Only load wingman once, we'll duplicate it later
            loadTasks.push_back({m_config.wingmanModelPath, "wingman", false});
        }
        
        if (!m_config.missileModelPath.empty())
        {
            loadTasks.push_back({m_config.missileModelPath, "missile", false});
        }
        
        if (m_config.enableAncientCity && !m_config.ancientCityModelPath.empty())
        {
            loadTasks.push_back({m_config.ancientCityModelPath, "ancientCity", true});
        }
        
        if (m_config.enableLanterns && !m_config.lanternModelPath.empty())
        {
            loadTasks.push_back({m_config.lanternModelPath, "kongming", false});
        }
        
        // Flagpole is now procedurally generated, no need to load model
        
        // Load all models in parallel
        if (!loadTasks.empty())
        {
            log(LogLevel::Info, "Starting parallel loading of " + std::to_string(loadTasks.size()) + " model files...");
            std::chrono::high_resolution_clock::time_point loadStart = std::chrono::high_resolution_clock::now();
            
            // Structure to hold loaded model data
            struct LoadedModel
            {
                std::string name;
                std::optional<Mesh> singleMesh;
                std::vector<Mesh> multipleMeshes;
                bool isMeshes;
                bool success;
            };
            
            // Launch async tasks for each model
            std::vector<std::future<LoadedModel>> futures;
            for (const auto& task : loadTasks)
            {
                futures.push_back(std::async(std::launch::async, [task]() -> LoadedModel {
                    LoadedModel result;
                    result.name = task.name;
                    result.isMeshes = task.isMeshes;
                    result.success = false;
                    
                    try
                    {
                        if (task.isMeshes)
                        {
                            // Try to load from explicit path first
                            namespace fs = std::filesystem;
                            if (fs::exists(task.path))
                            {
                                result.multipleMeshes = cg::loadObjAsMeshes(task.path);
                                result.success = !result.multipleMeshes.empty();
                            }
                            
                            // If that failed, try to find any .obj file in the directory
                            if (!result.success)
                            {
                                fs::path modelDir = fs::path(task.path).parent_path();
                                if (fs::is_directory(modelDir))
                                {
                                    cg::log(cg::LogLevel::Warn, "Failed to load " + task.name + " from explicit path: " + task.path + ", searching directory...");
                                    for (const auto& entry : fs::directory_iterator(modelDir))
                                    {
                                        if (entry.is_regular_file() && entry.path().extension() == ".obj")
                                        {
                                            cg::log(cg::LogLevel::Info, "Trying to load " + task.name + " from: " + entry.path().string());
                                            result.multipleMeshes = cg::loadObjAsMeshes(entry.path().string());
                                            if (!result.multipleMeshes.empty())
                                            {
                                                result.success = true;
                                                cg::log(cg::LogLevel::Info, "Successfully loaded " + task.name + " from: " + entry.path().string());
                                                break;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    cg::log(cg::LogLevel::Error, "Model directory does not exist: " + modelDir.string());
                                }
                            }
                        }
                        else
                        {
                            result.singleMesh = cg::loadObjAsMesh(task.path);
                            result.success = result.singleMesh.has_value();
                        }
                    }
                    catch (const std::exception& e)
                    {
                        cg::log(cg::LogLevel::Error, "Exception while loading " + task.name + ": " + e.what());
                        result.success = false;
                    }
                    
                    return result;
                }));
            }
            
            // Wait for all models to load and collect results
            std::vector<LoadedModel> loadedModels;
            for (auto& future : futures)
            {
                loadedModels.push_back(future.get());
            }
            
            std::chrono::high_resolution_clock::time_point loadEnd = std::chrono::high_resolution_clock::now();
            long long loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count();
            log(LogLevel::Info, "Model loading completed in " + std::to_string(loadTime) + "ms");
            
            // Process loaded models and add to scene
            for (auto& loaded : loadedModels)
            {
                if (!loaded.success)
                {
                    log(LogLevel::Error, "Failed to load " + loaded.name + " model");
                    if (loaded.name == "airplane" || loaded.name == "missile")
                    {
                        return false;  // Critical models
                    }
                    continue;
                }
                
                if (loaded.name == "airplane")
                {
                    auto airplane = *loaded.singleMesh;
                    airplane.name = "airplane";
                    const glm::vec3 startPos = m_config.airplaneStartPosition + glm::vec3(0.0f, m_config.airplaneHeight, 0.0f);
                    glm::mat4 transform(1.0f);
                    transform = glm::translate(transform, startPos);
                    const glm::vec3 forward = glm::normalize(m_config.airplaneDirection);
                    const float yaw = glm::degrees(atan2f(forward.z, forward.x));
                    transform = glm::rotate(transform, glm::radians(yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    transform = applyScale(transform, m_config.airplaneScale);
                    if (m_config.airplaneSpawnTime > 0.0f)
                    {
                        transform = applyScale(transform, glm::vec3(0.0f, 0.0f, 0.0f));
                    }
                    airplane.transform = transform;
                    meshes.push_back(std::move(airplane));
                    log(LogLevel::Info, "Airplane model processed (will spawn at " + std::to_string(m_config.airplaneSpawnTime) + "s)");
                }
                else if (loaded.name == "wingman")
                {
                    // Load once, duplicate 4 times
                    auto baseWingman = *loaded.singleMesh;
                    glm::mat4 baseTransform(1.0f);
                    if (m_config.airplaneSpawnTime > 0.0f)
                    {
                        baseTransform = applyScale(baseTransform, glm::vec3(0.0f, 0.0f, 0.0f));
                    }
                    
                    // Left side wingmen
                    for (int i = 0; i < 2; ++i)
                    {
                        auto wingman = baseWingman;  // Copy mesh
                        wingman.name = "wingman_left" + std::to_string(i + 1);
                        wingman.transform = baseTransform;
                        meshes.push_back(std::move(wingman));
                    }
                    
                    // Right side wingmen
                    for (int i = 0; i < 2; ++i)
                    {
                        auto wingman = baseWingman;  // Copy mesh
                        wingman.name = "wingman_right" + std::to_string(i + 1);
                        wingman.transform = baseTransform;
                        meshes.push_back(std::move(wingman));
                    }
                    
                    log(LogLevel::Info, "Wingman models processed (4 wingmen)");
                }
                else if (loaded.name == "missile")
                {
                    auto missile = *loaded.singleMesh;
                    missile.name = "missile";
                namespace fs = std::filesystem;
                fs::path objDir = fs::path(m_config.missileModelPath).parent_path();
                fs::path bodyTex = objDir / "body.bmp";
                if (fs::exists(bodyTex))
                {
                        missile.diffuseTexture = bodyTex.generic_string();
                }
                glm::mat4 transform(1.0f);
                transform = applyScale(transform, glm::vec3(0.0f, 0.0f, 0.0f));
                    missile.transform = transform;
                    meshes.push_back(std::move(missile));
                    log(LogLevel::Info, "Missile model processed");
                }
                else if (loaded.name == "ancientCity")
                {
                    glm::mat4 transform(1.0f);
                    transform = glm::translate(transform, m_config.ancientCityPosition);
                    if (m_config.ancientCityRotationY != 0.0f)
                    {
                        transform = glm::rotate(transform, glm::radians(m_config.ancientCityRotationY), glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    transform = applyScale(transform, m_config.ancientCityScale);
                    
                    for (auto& mesh : loaded.multipleMeshes)
                    {
                        mesh.name = "ancientCity_" + mesh.name;
                        mesh.transform = transform;
                        meshes.push_back(std::move(mesh));
                    }
                    
                    log(LogLevel::Info, "Ancient city model processed as " + std::to_string(loaded.multipleMeshes.size()) + 
                        " meshes at position (" + 
                        std::to_string(m_config.ancientCityPosition.x) + ", " + 
                        std::to_string(m_config.ancientCityPosition.y) + ", " + 
                        std::to_string(m_config.ancientCityPosition.z) + ")");
                }
                else if (loaded.name == "kongming")
                {
                    if (loaded.singleMesh)
                    {
                        m_lanternPrototype = *loaded.singleMesh;
                        m_lanternPrototypeLoaded = true;
                        log(LogLevel::Info, "Loaded kongming lantern prototype.");
                    }
                }
                // Flagpole is now procedurally generated, no model loading needed
            }
        }
        
        // Generate procedural flagpole (cylinder + sphere)
        if (m_config.enableFlagpole)
        {
            log(LogLevel::Info, "Generating procedural flagpole...");
            auto flagpoleMeshes = FlagGenerator::generateFlagpole(
                m_config.flagpoleHeight,
                m_config.flagpoleRadius,
                m_config.flagpoleBallRadius,
                m_config.flagpoleSegments,
                m_config.flagpoleColor,
                m_config.flagpoleBallColor
            );
            
            // Apply position transform to all flagpole meshes
            glm::mat4 flagpoleTransform = glm::translate(glm::mat4(1.0f), m_config.flagpolePosition);
            for (auto& mesh : flagpoleMeshes)
            {
                mesh.transform = flagpoleTransform;
                meshes.push_back(std::move(mesh));
            }
            
            log(LogLevel::Info, "Flagpole generated: " + std::to_string(flagpoleMeshes.size()) + 
                " meshes (pole + ball), height=" + std::to_string(m_config.flagpoleHeight) +
                ", position (" + 
                std::to_string(m_config.flagpolePosition.x) + ", " + 
                std::to_string(m_config.flagpolePosition.y) + ", " + 
                std::to_string(m_config.flagpolePosition.z) + ")");
        }
        
        // Create flag mesh if enabled
        if (m_config.enableFlag && m_config.enableFlagpole)
        {
            log(LogLevel::Info, "Generating Bezier flag mesh...");
            Mesh flag = generateBezierFlag(
                m_config.flagWidth,
                m_config.flagHeight,
                m_config.flagControlPointsU,
                m_config.flagControlPointsV,
                m_config.flagSegmentsU,
                m_config.flagSegmentsV
            );
            
            // Position flag so its top-left corner aligns with the bottom center of the ball
            // Flag's local coordinate: top-left is (-halfWidth, halfHeight, 0)
            // Ball's bottom center in world: (0, flagpoleHeight, 0) relative to flagpolePosition
            float halfWidth = m_config.flagWidth * 0.5f;
            float halfHeight = m_config.flagHeight * 0.5f;
            glm::mat4 flagTransform(1.0f);
            flagTransform = glm::translate(flagTransform, 
                m_config.flagpolePosition + glm::vec3(halfWidth, m_config.flagpoleHeight - halfHeight, 0.0f));
            flag.transform = flagTransform;
            flag.name = "flag";
            flag.diffuseTexture = m_config.flagTexturePath;
            
            meshes.push_back(std::move(flag));
            m_flagExists = true;
            log(LogLevel::Info, "Flag mesh created and positioned at flagpole top");
            
            if (m_config.debugShowFlagControlPoints)
            {
                Mesh controlPointMesh = FlagGenerator::generateFlagControlPointDebugMesh(
                    m_config.flagWidth,
                    m_config.flagHeight,
                    m_config.flagControlPointsU,
                    m_config.flagControlPointsV,
                    m_flagControlPointMarkerSize,
                    m_flagControlPointColor
                );
                controlPointMesh.transform = flagTransform;
                controlPointMesh.name = "flag_control_points";
                meshes.push_back(std::move(controlPointMesh));
                m_flagControlPointMeshExists = true;
                log(LogLevel::Info, "Flag control point debug mesh enabled");
            }
        }

        if (m_config.enableLanterns && m_lanternPrototypeLoaded)
        {
            const int poolSize = std::max(0, m_config.lanternPoolSize);
            m_lanternMeshNames.reserve(poolSize);
            // Use OBJ model as-is, no texture override
            glm::vec3 lanternBaseColor = m_config.lanternLightColor;
            for (int i = 0; i < poolSize; ++i)
            {
                Mesh lanternInstance = *m_lanternPrototype;
                lanternInstance.name = "lantern_" + std::to_string(i);
                lanternInstance.transform = applyScale(glm::mat4(1.0f), glm::vec3(0.0f));
                // Keep original texture from OBJ if it has one, otherwise use vertex colors
                // Set warm orange/yellow vertex colors as base for internal light effect
                if (lanternInstance.vertices.size() > 0)
                {
                    for (auto& vertex : lanternInstance.vertices)
                    {
                        // Preserve original color but add warm tint
                        vertex.color = glm::vec4(
                            glm::mix(glm::vec3(vertex.color), lanternBaseColor, 0.3f),
                            1.0f
                        );
                    }
                }
                meshes.push_back(lanternInstance);
                m_lanternMeshNames.push_back(lanternInstance.name);
            }
            log(LogLevel::Info, "Lantern pool created with " + std::to_string(poolSize) + " instances.");
        }

        // Create scene and renderer
        log(LogLevel::Info, "Creating scene and renderer...");
        m_scene = std::make_unique<Scene>(std::move(meshes));
        m_renderer = std::make_unique<SceneRenderer>(*m_scene);
        MaterialFeatureToggles materialToggles{};
        materialToggles.flagpoleMetal = m_config.enableFlagpoleMetalMaterial;
        materialToggles.missileMetal = m_config.enableMissileMetalMaterial;
        materialToggles.groundTriplanar = m_config.enableGroundProceduralMapping;
        materialToggles.flagAnisotropic = m_config.enableFlagClothAnisotropy;
        m_renderer->setAdvancedMaterialToggles(materialToggles);
        
        // Configure texture quality settings
        m_renderer->setTextureAnisotropyLevel(m_config.textureAnisotropyLevel);
        m_renderer->setTextureQualityDistances(
            m_config.textureQualityNearDistance,
            m_config.textureQualityFarDistance,
            m_config.textureQualityMinFactor
        );
        log(LogLevel::Info, "Texture anisotropy level set to " + std::to_string(m_config.textureAnisotropyLevel) + "x");
        log(LogLevel::Info, "Texture quality distances: near=" + std::to_string(m_config.textureQualityNearDistance) + 
            ", far=" + std::to_string(m_config.textureQualityFarDistance) + 
            ", minFactor=" + std::to_string(m_config.textureQualityMinFactor));

        // Load skybox textures
        log(LogLevel::Info, "Loading skybox textures...");
        m_skybox = std::make_unique<SkyboxRenderer>();
        if (!m_skybox->loadEquirectangularTextures(m_config.daySkyboxPath, m_config.nightSkyboxPath))
        {
            log(LogLevel::Error, "Failed to load skybox textures.");
            return false;
        }
        m_skybox->setNightBrightness(m_config.nightSkyboxBrightness);
        if (m_renderer)
        {
            m_renderer->setEnvironmentMaps(m_skybox->dayTextureHandle(), m_skybox->nightTextureHandle());
        }
        log(LogLevel::Info, "Skybox textures loaded successfully");

        // Initialize text renderer
        log(LogLevel::Info, "Initializing text renderer...");
        m_textRenderer = std::make_unique<TextRenderer>();

        // Initialize particle system
        const bool needsParticleSystem = (m_config.enableAirplaneTrails && m_config.airplaneTrailMaxParticles > 0) ||
                                         (m_config.enableMissileExplosion && m_config.missileExplosionParticleCount > 0);
        if (needsParticleSystem)
        {
            int desiredMax = std::max(m_config.airplaneTrailMaxParticles, m_config.missileExplosionParticleCount);
            desiredMax = std::max(desiredMax, 200);
            const size_t maxParticles = static_cast<size_t>(desiredMax);
            m_particleSystem = std::make_unique<ParticleSystem>(maxParticles);
            log(LogLevel::Info, "Particle system initialized with max " + std::to_string(maxParticles) + " particles");
        }

        // Normalize airplane direction
        m_normalizedAirplaneDirection = glm::normalize(m_config.airplaneDirection);

        // Set up camera - use default position initially
        log(LogLevel::Info, "Setting up camera...");
        
        // Calculate airplane position for reference (used for tracking calculations)
        const glm::vec3 airplanePos = m_config.airplaneStartPosition + glm::vec3(0.0f, m_config.airplaneHeight, 0.0f);
        
        // Save initial camera position (default position) - this will be used when tracking airplane without following position
        m_initialCameraPosition = m_config.defaultCameraPosition;
        m_initialCameraTarget = m_config.defaultCameraTarget;
        
        // Set camera to default position (not following airplane initially)
        // Only set if camera motion is disabled, otherwise let motion system handle it
        if (!m_config.enableCameraMotion)
        {
            m_camera.setPosition(m_config.defaultCameraPosition);
            m_camera.lookAt(m_config.defaultCameraTarget);
        }
        m_camera.setFOV(m_config.defaultFOV);
        
        // Initialize airplane position
        m_airplanePosition = airplanePos;
        // Airplane will be activated at spawnTime (if spawnTime <= 0, start immediately)
        m_airplaneActive = (m_config.airplaneSpawnTime <= 0.0f);
        m_airplaneHasSpawned = false;
        m_airplaneSpawnTime = 0.0;

        // Initialize missile state
        m_missileActive = false;
        m_missileHasSpawned = false;
        m_missileSpawnTime = 0.0;
        m_missilePosition = glm::vec3(0.0f);
        m_missileVelocity = glm::vec3(0.0f);
        m_missileRotationAngle = 0.0f;
        m_reachedKeyframe3 = false;
        m_missileExploded = false;
        m_missileExplosionTime = 0.0;
        m_missileExplosionPosition = glm::vec3(0.0f);
        m_resumingToKeyframe4 = false;
        m_airplaneDisappearTime = 0.0;
        m_cameraPositionWhenAirplaneDisappeared = glm::vec3(0.0f);
        m_cameraYawWhenAirplaneDisappeared = 0.0f;
        m_cameraPitchWhenAirplaneDisappeared = 0.0f;

        if (m_config.enableLanterns && !m_lanternMeshNames.empty())
        {
            m_lanternInstances.clear();
            m_lanternInstances.resize(m_lanternMeshNames.size());
            for (size_t i = 0; i < m_lanternInstances.size(); ++i)
            {
                m_lanternInstances[i].meshName = m_lanternMeshNames[i];
            }
        }

        return true;
    }

    void App::processInput(double deltaSeconds)
    {
        constexpr float rotationSensitivity = 0.08f;

        // Disable manual camera control when camera motion system is enabled
        if (m_config.enableCameraMotion)
        {
            m_input.cursorDelta = glm::vec2(0.0f);
            m_input.scrollDelta = 0.0f;
            return;
        }

        // Disable manual camera control when missile is active (prioritize missile tracking)
        if (m_missileActive)
        {
            m_input.cursorDelta = glm::vec2(0.0f);
            m_input.scrollDelta = 0.0f;
            return;
        }
        
        // Disable manual camera control when airplane is active and tracking is enabled
        if (m_airplaneActive && m_config.enableAirplaneCameraTracking)
        {
            m_input.cursorDelta = glm::vec2(0.0f);
            m_input.scrollDelta = 0.0f;
            return;
        }

        if (m_input.freeLook)
        {
            m_camera.rotate(m_input.cursorDelta.x * rotationSensitivity, -m_input.cursorDelta.y * rotationSensitivity);
        }

        m_input.cursorDelta = glm::vec2(0.0f);

        m_moveSpeed = glm::clamp(m_moveSpeed + m_input.scrollDelta * 50.0f, 10.0f, 5000.0f);
        m_input.scrollDelta = 0.0f;

        if (!m_input.freeLook)
        {
            return;
        }

        glm::vec3 velocity{0.0f};
        if (m_input.forward) velocity += m_camera.forward();
        if (m_input.backward) velocity -= m_camera.forward();
        if (m_input.left) velocity -= m_camera.right();
        if (m_input.right) velocity += m_camera.right();
        if (m_input.up) velocity += glm::vec3(0.0f, 1.0f, 0.0f);
        if (m_input.down) velocity -= glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::length(velocity) > 0.0f)
        {
            velocity = glm::normalize(velocity);
            const float speed = m_moveSpeed * static_cast<float>(m_input.boost ? 2.5f : 1.0f);
            m_camera.move(velocity * speed * static_cast<float>(deltaSeconds));
        }
    }

    void App::updateSkyBlend(double deltaSeconds)
    {
        // Calculate cycle duration from phase durations
        const double dayDuration = static_cast<double>(m_config.skyDayDuration);
        const double dayToNightTransition = static_cast<double>(m_config.skyDayToNightTransition);
        const double nightDuration = static_cast<double>(m_config.skyNightDuration);
        const double nightToDayTransition = static_cast<double>(m_config.skyNightToDayTransition);
        const double cycleDuration = dayDuration + dayToNightTransition + nightDuration + nightToDayTransition;

        if (cycleDuration <= 0.0)
        {
            m_skyBlend = 0.0f;
            return;
        }

        m_skyTime += deltaSeconds;
        
        // Wrap time within cycle
        double cycleTime = std::fmod(m_skyTime, cycleDuration);
        if (cycleTime < 0.0)
        {
            cycleTime += cycleDuration;
        }

        // Determine current phase and calculate blend value
        if (cycleTime < dayDuration)
        {
            // Day phase: blend = 0
            m_skyBlend = 0.0f;
        }
        else if (cycleTime < dayDuration + dayToNightTransition)
        {
            // Day to night transition: blend from 0 to 1
            const double transitionTime = cycleTime - dayDuration;
            const double normalized = transitionTime / dayToNightTransition;
            m_skyBlend = static_cast<float>(std::clamp(normalized, 0.0, 1.0));
        }
        else if (cycleTime < dayDuration + dayToNightTransition + nightDuration)
        {
            // Night phase: blend = 1
            m_skyBlend = 1.0f;
        }
        else
        {
            // Night to day transition: blend from 1 to 0
            const double transitionTime = cycleTime - (dayDuration + dayToNightTransition + nightDuration);
            const double normalized = 1.0 - (transitionTime / nightToDayTransition);
            m_skyBlend = static_cast<float>(std::clamp(normalized, 0.0, 1.0));
        }
    }

    void App::updateAirplaneAnimation(double deltaSeconds)
    {
        // Check if it's time to spawn the airplane (only spawn once)
        if (!m_airplaneHasSpawned && m_totalTime >= static_cast<double>(m_config.airplaneSpawnTime))
        {
            m_airplaneActive = true;
            m_airplaneHasSpawned = true;
            m_airplaneSpawnTime = m_totalTime;
            log(LogLevel::Info, "Airplane spawned at time " + std::to_string(m_totalTime) + "s");
        }

        // If airplane is not active yet, don't update it
        if (!m_airplaneActive)
        {
            return;
        }

        // Check if airplane should despawn (if lifetime is set)
        if (m_config.airplaneLifetime > 0.0f)
        {
            const double elapsedSinceSpawn = m_totalTime - m_airplaneSpawnTime;
            if (elapsedSinceSpawn > static_cast<double>(m_config.airplaneLifetime))
            {
                // Airplane lifetime expired - destroy it
                m_airplaneActive = false;
                m_airplaneDisappearTime = m_totalTime;  // Record when airplane disappeared
                
                // Record current camera position and rotation for smooth transition
                // This happens after updateCameraMotion has set the camera to look at explosion,
                // so we record the actual current state (keyframe 4 position + looking at explosion)
                m_cameraPositionWhenAirplaneDisappeared = m_camera.position();
                m_cameraYawWhenAirplaneDisappeared = m_camera.yaw();
                m_cameraPitchWhenAirplaneDisappeared = m_camera.pitch();
                
                // Mark that we should resume motion to keyframe 5
                // This will be checked in the next frame's updateCameraMotion
                if (m_missileExploded)
                {
                    m_resumingToKeyframe4 = true;
                }
                
                log(LogLevel::Info, "Airplane destroyed after " + std::to_string(m_config.airplaneLifetime) + "s lifetime");
                
                // Hide airplane and wingmen by scaling to zero (destroy them visually)
                glm::mat4 destroyTransform(1.0f);
                destroyTransform = applyScale(destroyTransform, glm::vec3(0.0f, 0.0f, 0.0f));
                m_renderer->setMeshTransformByName("airplane", destroyTransform);
                m_renderer->setMeshTransformByName("wingman_left1", destroyTransform);
                m_renderer->setMeshTransformByName("wingman_left2", destroyTransform);
                m_renderer->setMeshTransformByName("wingman_right1", destroyTransform);
                m_renderer->setMeshTransformByName("wingman_right2", destroyTransform);
                
                // Don't restore camera to default position if camera motion is enabled
                // Let camera motion system handle camera position
                if (m_config.enableAirplaneCameraTracking && !m_config.enableCameraMotion)
                {
                    m_camera.setPosition(m_config.defaultCameraPosition);
                    m_camera.lookAt(m_config.defaultCameraTarget);
                }
                return;
            }
        }

        const float speed = m_config.airplaneSpeed;
        const float height = m_config.airplaneHeight;

        // Update airplane position based on time since spawn
        const glm::vec3 basePosition = m_config.airplaneStartPosition;
        // Calculate time elapsed since actual spawn time
        const double timeSinceSpawn = m_totalTime - m_airplaneSpawnTime;
        const glm::vec3 movement = static_cast<float>(timeSinceSpawn) * speed * m_normalizedAirplaneDirection;
        m_airplanePosition = basePosition + glm::vec3(0.0f, height, 0.0f) + movement;

        // Calculate wingman positions relative to main airplane (in V-formation)
        // Transform formation offsets from local space to world space
        const glm::vec3 right = glm::normalize(glm::cross(m_normalizedAirplaneDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 forward = m_normalizedAirplaneDirection;
        
        // Left side wingmen
        m_wingmanLeft1Position = m_airplanePosition + 
            right * m_config.wingmanLeft1Offset.x + 
            up * m_config.wingmanLeft1Offset.y + 
            forward * m_config.wingmanLeft1Offset.z;
        m_wingmanLeft2Position = m_airplanePosition + 
            right * m_config.wingmanLeft2Offset.x + 
            up * m_config.wingmanLeft2Offset.y + 
            forward * m_config.wingmanLeft2Offset.z;
        
        // Right side wingmen
        m_wingmanRight1Position = m_airplanePosition + 
            right * m_config.wingmanRight1Offset.x + 
            up * m_config.wingmanRight1Offset.y + 
            forward * m_config.wingmanRight1Offset.z;
        m_wingmanRight2Position = m_airplanePosition + 
            right * m_config.wingmanRight2Offset.x + 
            up * m_config.wingmanRight2Offset.y + 
            forward * m_config.wingmanRight2Offset.z;

        // Update airplane transform
        glm::mat4 transform(1.0f);
        transform = glm::translate(transform, m_airplanePosition);
        
        // Calculate rotation to face direction (reuse forward variable from above)
        const float yaw = glm::degrees(atan2f(forward.z, forward.x));
        transform = glm::rotate(transform, glm::radians(yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate 90 degrees horizontally
        transform = applyScale(transform, m_config.airplaneScale);
        
        // Update airplane transform
        bool transformSet = m_renderer->setMeshTransformByName("airplane", transform);
        if (!transformSet)
        {
            static bool warned = false;
            if (!warned)
            {
                log(LogLevel::Error, "Failed to update airplane transform - mesh 'airplane' not found!");
                warned = true;
            }
        }

        // Update wingman transforms (same rotation as main airplane)
        // Reuse yaw calculation from above
        const float yawAngle = yaw;
        
        // Helper function to update wingman transform
        auto updateWingmanTransform = [&](const std::string& name, const glm::vec3& position) {
            glm::mat4 wingmanTransform(1.0f);
            wingmanTransform = glm::translate(wingmanTransform, position);
            wingmanTransform = glm::rotate(wingmanTransform, glm::radians(yawAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            // Add 90 degree rotation for wingmen
            wingmanTransform = glm::rotate(wingmanTransform, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            wingmanTransform = applyScale(wingmanTransform, m_config.wingmanScale);
            
            if (!m_renderer->setMeshTransformByName(name, wingmanTransform))
            {
                static std::unordered_map<std::string, bool> warned;
                if (!warned[name])
                {
                    log(LogLevel::Error, "Failed to update wingman transform - mesh '" + name + "' not found!");
                    warned[name] = true;
                }
            }
        };
        
        // Update all 4 wingmen
        updateWingmanTransform("wingman_left1", m_wingmanLeft1Position);
        updateWingmanTransform("wingman_left2", m_wingmanLeft2Position);
        updateWingmanTransform("wingman_right1", m_wingmanRight1Position);
        updateWingmanTransform("wingman_right2", m_wingmanRight2Position);

        // Update camera to track airplane (only when airplane is active and missile is not active and not exploded)
        if (m_config.enableAirplaneCameraTracking && m_airplaneActive && !m_missileActive && !m_missileExploded)
        {
            if (m_config.airplaneCameraFollowPosition)
            {
                // Camera position follows airplane
                const glm::vec3 backward = -m_normalizedAirplaneDirection;
                const glm::vec3 cameraPos = m_airplanePosition + backward * m_config.airplaneCameraDistance +
                                            glm::vec3(0.0f, m_config.airplaneCameraHeight, 0.0f);
                m_camera.setPosition(cameraPos);
            }
            else
            {
                // Camera position stays at default position, only look at airplane
                // Don't change camera position, just update the look-at target
            }
            // Always look at airplane position while active
            m_camera.lookAt(m_airplanePosition);
        }
    }

    void App::updateMissileAnimation(double deltaSeconds)
    {
        // Check if it's time to drop missile from airplane (3 seconds after airplane spawn)
        if (!m_missileHasSpawned && m_airplaneHasSpawned && m_airplaneActive)
        {
            const double timeSinceAirplaneSpawn = m_totalTime - m_airplaneSpawnTime;
            if (timeSinceAirplaneSpawn >= static_cast<double>(m_config.missileDropTime))
            {
                // Drop missile from airplane
                m_missileActive = true;
                m_missileHasSpawned = true;
                m_missileSpawnTime = m_totalTime;
                m_missilePosition = m_airplanePosition;  // Start at airplane position
                
                // Calculate missile velocity: horizontal component from airplane direction + vertical fall
                const float fallAngleRad = glm::radians(m_config.missileFallAngle);
                const float horizontalSpeed = m_config.missileFallSpeed * cosf(fallAngleRad);
                const float verticalSpeed = -m_config.missileFallSpeed * sinf(fallAngleRad);  // Negative for downward
                
                // Horizontal velocity in airplane's forward direction
                glm::vec3 horizontalVel = m_normalizedAirplaneDirection * horizontalSpeed;
                // Vertical velocity downward
                m_missileVelocity = horizontalVel + glm::vec3(0.0f, verticalSpeed, 0.0f);
                
                // Reset rotation angle when missile is dropped
                m_missileRotationAngle = 0.0f;
                
                log(LogLevel::Info, "Missile dropped from airplane at time " + std::to_string(m_totalTime) + "s");
            }
        }

        // If missile is not active yet, don't update it
        if (!m_missileActive)
        {
            return;
        }

        // Update missile position
        m_missilePosition += m_missileVelocity * static_cast<float>(deltaSeconds);

        // Update missile rotation around forward axis
        m_missileRotationAngle += m_config.missileRotationSpeed * static_cast<float>(deltaSeconds);
        // Wrap angle to [0, 360) for numerical stability
        while (m_missileRotationAngle >= 360.0f)
        {
            m_missileRotationAngle -= 360.0f;
        }

        // Check if missile hits ground
        if (m_missilePosition.y <= m_config.groundHeight)
        {
            // Missile hit ground - destroy it
            m_missileActive = false;
            m_missileExploded = true;
            m_missileExplosionTime = m_totalTime;
            m_missileExplosionPosition = glm::vec3(m_missilePosition.x, m_config.groundHeight, m_missilePosition.z);
            triggerMissileExplosion(m_missileExplosionPosition);
            log(LogLevel::Info, "Missile hit ground at time " + std::to_string(m_totalTime) + "s, position (" + 
                std::to_string(m_missilePosition.x) + ", " + 
                std::to_string(m_missilePosition.y) + ", " + 
                std::to_string(m_missilePosition.z) + ")");
            
            // Hide missile by scaling to zero
            glm::mat4 destroyTransform(1.0f);
            destroyTransform = applyScale(destroyTransform, glm::vec3(0.0f, 0.0f, 0.0f));
            m_renderer->setMeshTransformByName("missile", destroyTransform);
            
            // Immediately return to keyframe 4 (original keyframe 3) and look at explosion position
            const auto& keyframe4 = m_config.cameraKeyframes[4];
            m_camera.setPosition(keyframe4.position);
            m_camera.lookAt(m_missileExplosionPosition);
            return;
        }

        // Update missile transform
        glm::mat4 transform(1.0f);
        transform = glm::translate(transform, m_missilePosition);
        
        // Calculate rotation to face velocity direction
        glm::vec3 forward = glm::normalize(m_missileVelocity);
        if (glm::length(forward) > 0.001f)
        {
            const float yaw = glm::degrees(atan2f(forward.z, forward.x));
            const float pitch = glm::degrees(asinf(-forward.y));  // Negative because y is up
            transform = glm::rotate(transform, glm::radians(yaw + 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            transform = glm::rotate(transform, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            
            // Apply rotation around forward axis (roll/spin)
            // Rotate around the forward direction (velocity direction) after orientation
            // Use Z-axis in model space
            transform = glm::rotate(transform, glm::radians(m_missileRotationAngle), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        
        transform = applyScale(transform, m_config.missileScale);
        
        // Update missile transform
        bool transformSet = m_renderer->setMeshTransformByName("missile", transform);
        if (!transformSet)
        {
            static bool warned = false;
            if (!warned)
            {
                log(LogLevel::Error, "Failed to update missile transform - mesh 'missile' not found!");
                warned = true;
            }
        }

        // Update camera to track missile (only when missile is active and delay has passed)
        if (m_missileActive)
        {
            // Check if delay has passed since missile was dropped
            const double timeSinceMissileDrop = m_totalTime - m_missileSpawnTime;
            if (timeSinceMissileDrop >= static_cast<double>(m_config.missileCameraTrackDelay))
            {
                // Camera follows missile, looking in the direction of movement
                const glm::vec3 forwardDir = glm::normalize(m_missileVelocity);
                
                // Calculate camera position - behind missile with configurable offset
                glm::vec3 cameraPos = m_missilePosition - forwardDir * m_config.missileCameraDistance + 
                                     glm::vec3(0.0f, m_config.missileCameraHeight, 0.0f);
                
                // Look ahead in the direction of movement
                glm::vec3 lookTarget = m_missilePosition + forwardDir * m_config.missileCameraLookAhead;
                
                m_camera.setPosition(cameraPos);
                m_camera.lookAt(lookTarget);
            }
        }
    }

    void App::updateFlagAnimation(double deltaSeconds)
    {
        if (!m_flagExists || !m_config.enableFlag)
        {
            return;
        }

        m_flagAnimationTime += static_cast<float>(deltaSeconds);

        if (m_flagUpdatePending)
        {
            if (m_flagUpdateFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                FlagUpdateResult result = m_flagUpdateFuture.get();
                m_renderer->updateMeshVerticesByName("flag", result.vertices);

                if (m_config.debugShowFlagControlPoints && m_flagControlPointMeshExists && !result.controlPoints.empty())
                {
                    m_flagControlPoints = result.controlPoints;
                    FlagGenerator::updateFlagControlPointDebugVertices(
                        m_flagControlPoints,
                        m_flagControlPointMarkerSize,
                        m_flagControlPointColor,
                        m_flagControlPointDebugVertices
                    );
                    m_renderer->updateMeshVerticesByName("flag_control_points", m_flagControlPointDebugVertices);
                }
                m_flagUpdatePending = false;
            }
        }

        if (!m_flagUpdatePending)
        {
            const float targetTime = m_flagAnimationTime;
            const bool captureControlPoints = m_config.debugShowFlagControlPoints && m_flagControlPointMeshExists;
            const float width = m_config.flagWidth;
            const float height = m_config.flagHeight;
            const int controlPointsU = m_config.flagControlPointsU;
            const int controlPointsV = m_config.flagControlPointsV;
            const int segmentsU = m_config.flagSegmentsU;
            const int segmentsV = m_config.flagSegmentsV;
            const float waveAmplitude = m_config.flagWaveAmplitude;
            const float waveFrequency = m_config.flagWaveFrequency;

            m_flagUpdateFuture = std::async(std::launch::async, [=]() -> FlagUpdateResult {
                FlagUpdateResult output;
                std::vector<glm::vec3> tempControlPoints;
                std::vector<Vertex> vertices = FlagGenerator::updateFlagVertices(
                    width,
                    height,
                    controlPointsU,
                    controlPointsV,
                    segmentsU,
                    segmentsV,
                    targetTime,
                    waveAmplitude,
                    waveFrequency,
                    captureControlPoints ? &tempControlPoints : nullptr
                );
                output.vertices = std::move(vertices);
                if (captureControlPoints)
                {
                    output.controlPoints = std::move(tempControlPoints);
                }
                return output;
            });

            m_flagUpdatePending = true;
        }
    }

    void App::updateLanterns(double deltaSeconds)
    {
        if (!m_config.enableLanterns || m_lanternInstances.empty() || !m_renderer)
        {
            if (m_renderer)
            {
                m_renderer->setLanternLights({});
            }
            return;
        }

        const float spawnStartTime = m_config.lanternSpawnStartTime;
        if (m_totalTime < static_cast<double>(spawnStartTime))
        {
            m_renderer->setLanternLights({});
            return;
        }

        m_lanternSpawnTimer += deltaSeconds;
        const float spawnInterval = m_config.lanternSpawnInterval;
        if (m_lanternSpawnTimer >= spawnInterval)
        {
            m_lanternSpawnTimer = 0.0;
            int minCount = m_config.lanternSpawnCountRange.x;
            int maxCount = std::max(m_config.lanternSpawnCountRange.y, minCount);
            int spawnCount = minCount;
            if (maxCount > minCount)
            {
                spawnCount = randomInt(minCount, maxCount);
            }
            spawnCount = std::max(spawnCount, 0);
            int available = static_cast<int>(std::count_if(m_lanternInstances.begin(), m_lanternInstances.end(),
                [](const LanternInstance& l) { return !l.active; }));
            spawnCount = std::min(spawnCount, available);
            for (int i = 0; i < spawnCount; ++i)
            {
                spawnLantern();
            }
        }

        std::vector<SceneRenderer::LanternLight> lights;
        lights.reserve(m_lanternInstances.size());

        for (auto& lantern : m_lanternInstances)
        {
            if (!lantern.active)
            {
                continue;
            }

            lantern.age += static_cast<float>(deltaSeconds);
            float t = lantern.age / lantern.duration;
            if (t >= 1.0f)
            {
                deactivateLantern(lantern);
                continue;
            }

            glm::vec3 position = evaluateLanternPosition(lantern, t);
            
            // For lanterns, we want them to stay upright (Y-axis up) but can tilt slightly
            // Model's default orientation: Y-up, Z-forward (or X-forward depending on model)
            // We'll keep Y-axis aligned with world Y-axis for upright orientation
            glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
            
            // Calculate a forward direction (can use tangent for slight tilt, or keep vertical)
            glm::vec3 tangent = evaluateLanternTangent(lantern, t);
            if (glm::length(tangent) < 0.001f)
            {
                tangent = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            tangent = glm::normalize(tangent);
            
            // Project tangent onto horizontal plane for forward direction
            glm::vec3 forward = tangent - worldUp * glm::dot(tangent, worldUp);
            if (glm::length(forward) < 0.001f)
            {
                forward = glm::vec3(0.0f, 0.0f, 1.0f); // Default forward if tangent is vertical
            }
            forward = glm::normalize(forward);
            
            // Right vector is cross product of forward and up
            glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
            // Recalculate forward to ensure orthogonality
            forward = glm::normalize(glm::cross(worldUp, right));

            glm::mat4 transform(1.0f);
            transform[0] = glm::vec4(right, 0.0f);      // X-axis: right
            transform[1] = glm::vec4(worldUp, 0.0f);    // Y-axis: up (always vertical)
            transform[2] = glm::vec4(-forward, 0.0f);   // Z-axis: forward (negative for OpenGL)
            transform[3] = glm::vec4(position, 1.0f);  // Position
            const glm::vec3 lanternScale = m_config.lanternScale;
            transform = applyScale(transform, lanternScale);

            m_renderer->setMeshTransformByName(lantern.meshName, transform);
            lantern.position = position;

            SceneRenderer::LanternLight light;
            // Light source is at the center of the lantern (not above it)
            light.position = position;
            light.color = m_config.lanternLightColor;
            light.intensity = m_config.lanternLightIntensity;
            light.radius = m_config.lanternLightRadius;
            lights.push_back(light);
        }

        m_renderer->setLanternLights(lights);
    }

    glm::vec3 App::evaluateLanternPosition(const LanternInstance& lantern, float t) const
    {
        float u = 1.0f - t;
        return u * u * u * lantern.p0 +
               3.0f * u * u * t * lantern.p1 +
               3.0f * u * t * t * lantern.p2 +
               t * t * t * lantern.p3;
    }

    glm::vec3 App::evaluateLanternTangent(const LanternInstance& lantern, float t) const
    {
        float u = 1.0f - t;
        return 3.0f * u * u * (lantern.p1 - lantern.p0) +
               6.0f * u * t * (lantern.p2 - lantern.p1) +
               3.0f * t * t * (lantern.p3 - lantern.p2);
    }

    void App::spawnLantern()
    {
        auto it = std::find_if(m_lanternInstances.begin(), m_lanternInstances.end(),
            [](const LanternInstance& lantern) { return !lantern.active; });
        if (it == m_lanternInstances.end())
        {
            return;
        }

        LanternInstance& lantern = *it;
        lantern.active = true;
        lantern.age = 0.0f;
        
        // Random speed for this lantern
        lantern.speed = randomFloat(m_config.lanternMinSpeed, m_config.lanternMaxSpeed);
        
        // Random lifetime
        lantern.duration = randomFloat(m_config.lanternMinLifetime, m_config.lanternMaxLifetime);

        // Spawn position: random horizontal position on the ground
        glm::vec3 spawnOffset(
            randomFloat(-m_config.lanternSpawnHalfExtents.x, m_config.lanternSpawnHalfExtents.x),
            0.0f,  // No vertical offset - spawn on ground
            randomFloat(-m_config.lanternSpawnHalfExtents.z, m_config.lanternSpawnHalfExtents.z));
        lantern.p0 = m_config.lanternSpawnCenter + spawnOffset;
        // Set initial point on the ground
        lantern.p0.y = 0.0f;

        // Calculate target height based on speed and lifetime
        // Faster lanterns will reach higher altitudes in the same time
        // Height = speed * lifetime (with some variation)
        float baseHeightDiff = lantern.speed * lantern.duration;
        float heightVariation = baseHeightDiff * 0.3f;  // 30% variation
        float heightDiff = baseHeightDiff + randomFloat(-heightVariation, heightVariation);
        
        // Clamp to target height range
        const float targetHeightMin = m_config.lanternTargetHeightMin;
        const float targetHeightMax = m_config.lanternTargetHeightMax;
        float minHeightDiff = targetHeightMin;
        float maxHeightDiff = targetHeightMax;
        heightDiff = glm::clamp(heightDiff, minHeightDiff, maxHeightDiff);
        
        // End point: random horizontal offset, height based on speed
        lantern.p3 = lantern.p0 + glm::vec3(
            randomFloat(-800.0f, 800.0f),
            heightDiff,  // Height difference based on speed
            randomFloat(-800.0f, 800.0f));

        // First control point: ensure it's above p0 and below p3
        // Use a fraction of the height difference to ensure upward motion
        float p1Height = lantern.p0.y + randomFloat(heightDiff * 0.3f, heightDiff * 0.7f);
        lantern.p1 = lantern.p0 + glm::vec3(
            randomFloat(-600.0f, 600.0f),
            p1Height - lantern.p0.y,  // Ensure p1.y is between p0.y and p3.y
            randomFloat(-600.0f, 600.0f));

        // Second control point: ensure it's above p0 and below p3
        // Use a fraction of the height difference to ensure upward motion
        float p2Height = lantern.p0.y + randomFloat(heightDiff * 0.5f, heightDiff * 0.9f);
        lantern.p2 = lantern.p0 + glm::vec3(
            randomFloat(-700.0f, 700.0f),
            p2Height - lantern.p0.y,  // Ensure p2.y is between p0.y and p3.y
            randomFloat(-700.0f, 700.0f));
        
        // Final verification: ensure p0.y < p1.y < p2.y < p3.y (strictly upward)
        // Sort control points by height to ensure monotonic upward motion
        if (lantern.p1.y < lantern.p0.y) lantern.p1.y = lantern.p0.y + 100.0f;
        if (lantern.p2.y < lantern.p1.y) lantern.p2.y = lantern.p1.y + 100.0f;
        if (lantern.p3.y < lantern.p2.y) lantern.p3.y = lantern.p2.y + 100.0f;

        lantern.position = lantern.p0;
        glm::mat4 transform(1.0f);
        transform = glm::translate(transform, lantern.position);
        const glm::vec3 lanternScale2 = m_config.lanternScale;
        transform = applyScale(transform, lanternScale2);
        m_renderer->setMeshTransformByName(lantern.meshName, transform);
    }

    void App::deactivateLantern(LanternInstance& lantern)
    {
        if (!lantern.active)
        {
            return;
        }
        lantern.active = false;
        lantern.age = 0.0f;
        lantern.duration = 0.0f;
        glm::mat4 transform(1.0f);
        transform = applyScale(transform, glm::vec3(0.0f));
        m_renderer->setMeshTransformByName(lantern.meshName, transform);
    }

    void App::updateCameraMotion(double deltaSeconds)
    {
        // Only update camera motion if enabled
        if (!m_config.enableCameraMotion)
        {
            return;
        }

        // Calculate cumulative time for each keyframe
        std::array<double, 6> keyframeTimes;
        keyframeTimes[0] = 0.0;
        for (size_t i = 0; i < 5; ++i)
        {
            keyframeTimes[i + 1] = keyframeTimes[i] + static_cast<double>(m_config.cameraTransitionTimes[i]);
        }

        const double keyframe4Time = keyframeTimes[4];  // Time when we reach keyframe 4 (original keyframe 3)

        // Phase 1: Normal motion from keyframe 0 to keyframe 4 (original keyframe 3)
        if (m_totalTime < keyframe4Time)
        {
            // Find which segment we're in (segments 0, 1, 2, 3: keyframe 0->1, 1->2, 2->3, 3->4)
            size_t currentSegment = 0;
            for (size_t i = 0; i < 4; ++i)
            {
                if (m_totalTime >= keyframeTimes[i] && m_totalTime <= keyframeTimes[i + 1])
                {
                    currentSegment = i;
                    break;
                }
                else if (m_totalTime > keyframeTimes[i + 1])
                {
                    currentSegment = i + 1;
                }
            }

            // Get the two keyframes for interpolation
            const auto& keyframe0 = m_config.cameraKeyframes[currentSegment];
            const auto& keyframe1 = m_config.cameraKeyframes[currentSegment + 1];

            // Calculate interpolation parameter t (0.0 to 1.0)
            const double segmentStartTime = keyframeTimes[currentSegment];
            const double segmentEndTime = keyframeTimes[currentSegment + 1];
            const double segmentDuration = segmentEndTime - segmentStartTime;
            
            double t = 0.0;
            if (segmentDuration > 0.0)
            {
                t = (m_totalTime - segmentStartTime) / segmentDuration;
                t = glm::clamp(t, 0.0, 1.0);
            }

            // Smooth interpolation using smoothstep for smoother motion
            const double smoothT = t * t * (3.0 - 2.0 * t);  // smoothstep function

            // Interpolate position
            const glm::vec3 interpolatedPos = glm::mix(
                keyframe0.position,
                keyframe1.position,
                static_cast<float>(smoothT)
            );

            // Interpolate yaw (handle 360-degree wrap)
            float yaw0 = keyframe0.yaw;
            float yaw1 = keyframe1.yaw;
            
            // Normalize yaw angles to [-180, 180] range
            while (yaw0 > 180.0f) yaw0 -= 360.0f;
            while (yaw0 < -180.0f) yaw0 += 360.0f;
            while (yaw1 > 180.0f) yaw1 -= 360.0f;
            while (yaw1 < -180.0f) yaw1 += 360.0f;
            
            // Find shortest path for yaw interpolation
            float yawDiff = yaw1 - yaw0;
            if (yawDiff > 180.0f)
            {
                yawDiff -= 360.0f;
            }
            else if (yawDiff < -180.0f)
            {
                yawDiff += 360.0f;
            }
            
            const float interpolatedYaw = yaw0 + yawDiff * static_cast<float>(smoothT);
            
            // Interpolate pitch
            const float interpolatedPitch = glm::mix(
                keyframe0.pitch,
                keyframe1.pitch,
                static_cast<float>(smoothT)
            );

            // Apply interpolated values to camera
            m_camera.setPosition(interpolatedPos);
            m_camera.setRotation(interpolatedYaw, interpolatedPitch);
            // Keep default FOV during normal motion
            m_camera.setFOV(m_config.defaultFOV);
            return;
        }

        // Phase 2: Reached keyframe 4 (original keyframe 3) - let airplane/missile tracking take over
        if (m_totalTime >= keyframe4Time && !m_missileExploded)
        {
            // Mark that we've reached keyframe 4 (original keyframe 3)
            if (!m_reachedKeyframe3)
            {
                const auto& keyframe4 = m_config.cameraKeyframes[4];
                m_camera.setPosition(keyframe4.position);
                m_camera.setRotation(keyframe4.yaw, keyframe4.pitch);
                m_camera.setFOV(m_config.defaultFOV);
                m_reachedKeyframe3 = true;
            }
            
            // Don't update camera motion while airplane or missile is active
            // Let airplane/missile tracking systems control the camera
            if (m_airplaneActive || m_missileActive)
            {
                return;
            }
            
            // If neither airplane nor missile is active, stay at keyframe 4
            const auto& keyframe4 = m_config.cameraKeyframes[4];
            m_camera.setPosition(keyframe4.position);
            m_camera.setRotation(keyframe4.yaw, keyframe4.pitch);
            m_camera.setFOV(m_config.defaultFOV);
            return;
        }

        // Phase 3: After missile explosion, look at explosion from keyframe 4 until airplane disappears
        if (m_missileExploded && !m_resumingToKeyframe4)
        {
            // Stay at keyframe 4 position and look at explosion position
            const auto& keyframe4 = m_config.cameraKeyframes[4];
            m_camera.setPosition(keyframe4.position);
            m_camera.lookAt(m_missileExplosionPosition);
            m_camera.setFOV(m_config.defaultFOV);  // Keep default FOV while looking at explosion
            
            // Wait until airplane disappears before resuming motion to keyframe 5
            // Don't set m_resumingToKeyframe4 here - let updateAirplaneAnimation handle it
            // This ensures camera state is recorded at the right moment
            
            return;
        }

        // Phase 4: After airplane disappears, resume motion from current camera position to keyframe 5
        if (m_missileExploded && m_resumingToKeyframe4)
        {
            // Calculate time elapsed since airplane disappeared
            const double timeSinceAirplaneDisappear = m_totalTime - m_airplaneDisappearTime;
            const double transition4To5Duration = static_cast<double>(m_config.cameraTransitionTimes[4]);
            
            // On the very first frame after airplane disappeared, ensure we're at the recorded position
            // This prevents any jump when transitioning from Phase 3 to Phase 4
            if (timeSinceAirplaneDisappear < 0.001)  // First frame after airplane disappeared
            {
                m_camera.setPosition(m_cameraPositionWhenAirplaneDisappeared);
                m_camera.setRotation(m_cameraYawWhenAirplaneDisappeared, m_cameraPitchWhenAirplaneDisappeared);
                m_camera.setFOV(m_config.defaultFOV);  // Start with default FOV
                return;  // Skip interpolation on first frame to ensure smooth transition
            }
            
            // Calculate interpolation parameter
            double t = 0.0;
            if (transition4To5Duration > 0.0)
            {
                t = timeSinceAirplaneDisappear / transition4To5Duration;
                t = glm::clamp(t, 0.0, 1.0);
            }

            // Smooth interpolation using smoothstep
            const double smoothT = t * t * (3.0 - 2.0 * t);

            // Interpolate from current camera position (when airplane disappeared) to keyframe 5
            const auto& keyframe5 = m_config.cameraKeyframes[5];

            // Interpolate position from camera position when airplane disappeared to keyframe 5
            const glm::vec3 interpolatedPos = glm::mix(
                m_cameraPositionWhenAirplaneDisappeared,
                keyframe5.position,
                static_cast<float>(smoothT)
            );

            // Interpolate yaw (handle 360-degree wrap)
            float yawStart = m_cameraYawWhenAirplaneDisappeared;
            float yaw5 = keyframe5.yaw;
            
            // Normalize yaw angles to [-180, 180] range
            while (yawStart > 180.0f) yawStart -= 360.0f;
            while (yawStart < -180.0f) yawStart += 360.0f;
            while (yaw5 > 180.0f) yaw5 -= 360.0f;
            while (yaw5 < -180.0f) yaw5 += 360.0f;
            
            // Find shortest path for yaw interpolation
            float yawDiff = yaw5 - yawStart;
            if (yawDiff > 180.0f)
            {
                yawDiff -= 360.0f;
            }
            else if (yawDiff < -180.0f)
            {
                yawDiff += 360.0f;
            }
            
            const float interpolatedYaw = yawStart + yawDiff * static_cast<float>(smoothT);
            
            // Interpolate pitch
            const float interpolatedPitch = glm::mix(
                m_cameraPitchWhenAirplaneDisappeared,
                keyframe5.pitch,
                static_cast<float>(smoothT)
            );

            // Interpolate FOV from default to final (wider angle)
            const float interpolatedFOV = glm::mix(
                m_config.defaultFOV,
                m_config.finalFOV,
                static_cast<float>(smoothT)
            );

            // Apply interpolated values to camera
            m_camera.setPosition(interpolatedPos);
            m_camera.setRotation(interpolatedYaw, interpolatedPitch);
            m_camera.setFOV(interpolatedFOV);
            
            // If we've reached keyframe 5, stay there
            if (t >= 1.0)
            {
                m_camera.setPosition(keyframe5.position);
                m_camera.setRotation(keyframe5.yaw, keyframe5.pitch);
                m_camera.setFOV(m_config.finalFOV);
            }
            
            return;
        }

        // Fallback: stay at last keyframe
        const auto& lastKeyframe = m_config.cameraKeyframes[5];
        m_camera.setPosition(lastKeyframe.position);
        m_camera.setRotation(lastKeyframe.yaw, lastKeyframe.pitch);
        m_camera.setFOV(m_config.finalFOV);
    }

    void App::updateParticleEffects(double deltaSeconds)
    {
        if (!m_particleSystem)
        {
            return;
        }

        m_particleSystem->update(static_cast<float>(deltaSeconds));

        if (!m_config.enableAirplaneTrails || !m_airplaneActive)
        {
            return;
        }

        glm::vec3 forward = m_normalizedAirplaneDirection;
        if (glm::dot(forward, forward) < 0.0001f)
        {
            forward = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        if (glm::dot(right, right) < 0.0001f)
        {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        emitTrailParticles(m_airplanePosition, forward, right, m_config.airplaneTrailRainbowColors[0], 0, deltaSeconds);
        emitTrailParticles(m_wingmanLeft1Position, forward, right, m_config.airplaneTrailRainbowColors[1], 1, deltaSeconds);
        emitTrailParticles(m_wingmanLeft2Position, forward, right, m_config.airplaneTrailRainbowColors[2], 2, deltaSeconds);
        emitTrailParticles(m_wingmanRight1Position, forward, right, m_config.airplaneTrailRainbowColors[3], 3, deltaSeconds);
        emitTrailParticles(m_wingmanRight2Position, forward, right, m_config.airplaneTrailRainbowColors[4], 4, deltaSeconds);
    }

    void App::emitTrailParticles(const glm::vec3& emitterPos, const glm::vec3& forward,
                                 const glm::vec3& right, const glm::vec4& color,
                                 size_t accumulatorIndex, double deltaSeconds)
    {
        if (!m_particleSystem || accumulatorIndex >= m_trailSpawnAccumulators.size())
        {
            return;
        }

        if (m_config.airplaneTrailSpawnRate <= 0.0f)
        {
            return;
        }

        float& accumulator = m_trailSpawnAccumulators[accumulatorIndex];
        accumulator += m_config.airplaneTrailSpawnRate * static_cast<float>(deltaSeconds);

        while (accumulator >= 1.0f)
        {
            accumulator -= 1.0f;

            ParticleSystem::SpawnParams params;
            glm::vec3 basePosition = emitterPos - forward * m_config.airplaneTrailEmissionOffset;
            basePosition += right * randomFloat(-m_config.airplaneTrailHorizontalJitter, m_config.airplaneTrailHorizontalJitter);
            basePosition += glm::vec3(0.0f, randomFloat(-m_config.airplaneTrailVerticalJitter, m_config.airplaneTrailVerticalJitter), 0.0f);
            params.position = basePosition;

            float speedVariation = randomFloat(-m_config.airplaneTrailSpeedVariance, m_config.airplaneTrailSpeedVariance);
            float baseSpeed = std::max(0.0f, m_config.airplaneTrailInitialSpeed + speedVariation);
            params.velocity = -forward * baseSpeed;
            params.velocity += right * randomFloat(-m_config.airplaneTrailLateralDrift, m_config.airplaneTrailLateralDrift);
            params.velocity += glm::vec3(0.0f, randomFloat(-m_config.airplaneTrailVerticalDrift, m_config.airplaneTrailVerticalDrift), 0.0f);

            params.color = color;
            params.startSize = m_config.airplaneTrailStartSize;
            params.endSize = m_config.airplaneTrailEndSize;
            params.lifetime = m_config.airplaneTrailParticleLifetime;
            params.acceleration = glm::vec3(0.0f, -std::abs(m_config.airplaneTrailGravity), 0.0f);

            m_particleSystem->emit(params);
        }
    }

    void App::triggerMissileExplosion(const glm::vec3& position)
    {
        if (!m_particleSystem || !m_config.enableMissileExplosion || m_config.missileExplosionParticleCount <= 0)
        {
            return;
        }

        const auto& palette = m_config.missileExplosionColors;
        const int paletteSize = static_cast<int>(palette.size());
        const float gravity = std::abs(m_config.missileExplosionGravity);

        for (int i = 0; i < m_config.missileExplosionParticleCount; ++i)
        {
            const glm::vec3 dir = randomUnitVector();
            const float speed = randomFloat(m_config.missileExplosionMinSpeed, m_config.missileExplosionMaxSpeed);
            const float lifetime = randomFloat(m_config.missileExplosionMinLifetime, m_config.missileExplosionMaxLifetime);
            const float startSize = randomFloat(m_config.missileExplosionStartSize * 0.8f, m_config.missileExplosionStartSize * 1.2f);
            const float endSize = randomFloat(m_config.missileExplosionEndSize * 0.6f, m_config.missileExplosionEndSize * 1.2f);

            ParticleSystem::SpawnParams params;
            params.position = position + dir * randomFloat(0.0f, 120.0f);
            params.velocity = dir * speed;
            params.acceleration = glm::vec3(0.0f, -gravity, 0.0f);
            params.lifetime = lifetime;
            params.startSize = startSize;
            params.endSize = endSize;

            glm::vec4 color = palette[paletteSize > 0 ? i % paletteSize : 0];
            glm::vec3 brightened = glm::mix(glm::vec3(color), glm::vec3(1.0f), randomFloat(0.0f, 0.25f));
            params.color = glm::vec4(brightened, color.a);

            m_particleSystem->emit(params);
        }
    }
} // namespace cg

