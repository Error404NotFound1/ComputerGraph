#pragma once

#include "core/AppConfig.h"
#include "core/GlfwContext.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "input/InputState.h"
#include "math/Camera.h"
#include "render/SceneRenderer.h"
#include "render/ParticleSystem.h"
#include "render/SkyboxRenderer.h"
#include "render/TextRenderer.h"
#include "scene/DemoSceneBuilder.h"
#include "scene/Scene.h"
#include <glm/gtc/random.hpp>

#include <memory>
#include <array>
#include <vector>
#include <future>
#include <optional>

namespace cg
{
    class App
    {
    public:
        explicit App(AppConfig config);
        int run();

    private:
        bool preloadResources();
        void processInput(double deltaSeconds);
        void updateSkyBlend(double deltaSeconds);
        void updateAirplaneAnimation(double deltaSeconds);
        void updateMissileAnimation(double deltaSeconds);
        void updateFlagAnimation(double deltaSeconds);
        void updateParticleEffects(double deltaSeconds);
        void updateCameraMotion(double deltaSeconds);
        void emitTrailParticles(const glm::vec3& emitterPos, const glm::vec3& forward,
                                const glm::vec3& right, const glm::vec4& color,
                                size_t accumulatorIndex, double deltaSeconds);
        void triggerMissileExplosion(const glm::vec3& position);
        struct FlagUpdateResult
        {
            std::vector<Vertex> vertices;
            std::vector<glm::vec3> controlPoints;
        };

        AppConfig m_config;
        GlfwContext m_glfwContext;
        std::unique_ptr<Window> m_window;
        InputState m_input{};
        std::unique_ptr<Scene> m_scene;
        std::unique_ptr<SceneRenderer> m_renderer;
        std::unique_ptr<SkyboxRenderer> m_skybox;
        std::unique_ptr<TextRenderer> m_textRenderer;
        Timer m_timer;
        Camera m_camera;
        float m_moveSpeed{600.0f};
        double m_skyTime{0.0};
        float m_skyBlend{0.0f};
        double m_lastCameraLogTime{0.0};  // Last time camera info was logged
        constexpr static double CAMERA_LOG_INTERVAL{0.5};  // Log camera info every 0.5 seconds
        
        // Camera motion state
        bool m_reachedKeyframe3{false};  // Track if we've reached keyframe 3
        bool m_missileExploded{false};  // Track if missile has exploded
        double m_missileExplosionTime{0.0};  // Time when missile exploded
        glm::vec3 m_missileExplosionPosition{0.0f};  // Position where missile exploded
        bool m_resumingToKeyframe4{false};  // Track if we're resuming motion from keyframe 3 to 4 after explosion
        double m_airplaneDisappearTime{0.0};  // Time when airplane disappeared (used for resuming motion)
        glm::vec3 m_cameraPositionWhenAirplaneDisappeared{0.0f};  // Camera position when airplane disappeared
        float m_cameraYawWhenAirplaneDisappeared{0.0f};  // Camera yaw when airplane disappeared
        float m_cameraPitchWhenAirplaneDisappeared{0.0f};  // Camera pitch when airplane disappeared

        std::unique_ptr<ParticleSystem> m_particleSystem;
        std::array<float, 5> m_trailSpawnAccumulators{};

        // Airplane animation state
        double m_totalTime{0.0};
        bool m_airplaneActive{false};
        double m_airplaneSpawnTime{0.0};  // Actual time when airplane was spawned
        bool m_airplaneHasSpawned{false};  // Track if airplane has spawned at least once
        glm::vec3 m_initialCameraPosition{};
        glm::vec3 m_initialCameraTarget{};
        glm::vec3 m_airplanePosition{};
        glm::vec3 m_normalizedAirplaneDirection{};
        
        // Wingman positions (calculated from main airplane position)
        glm::vec3 m_wingmanLeft1Position{};
        glm::vec3 m_wingmanLeft2Position{};
        glm::vec3 m_wingmanRight1Position{};
        glm::vec3 m_wingmanRight2Position{};

        // Missile animation state
        bool m_missileActive{false};
        bool m_missileHasSpawned{false};
        double m_missileSpawnTime{0.0};  // Actual time when missile was dropped
        glm::vec3 m_missilePosition{};
        glm::vec3 m_missileVelocity{};  // Missile velocity vector
        float m_missileRotationAngle{0.0f};  // Accumulated rotation angle around forward axis (degrees)
        
        // Flag animation state
        bool m_flagExists{false};  // Whether flag mesh exists in scene
        float m_flagAnimationTime{0.0f};  // Accumulated animation time for flag waving
        std::vector<glm::vec3> m_flagControlPoints;
        std::vector<Vertex> m_flagControlPointDebugVertices;
        bool m_flagControlPointMeshExists{false};
        float m_flagControlPointMarkerSize{8.0f};
        glm::vec3 m_flagControlPointColor{1.0f, 0.9f, 0.2f};
        std::future<FlagUpdateResult> m_flagUpdateFuture;
        bool m_flagUpdatePending{false};

        struct LanternInstance
        {
            bool active{false};
            float age{0.0f};
            float duration{0.0f};
            float speed{0.0f};  // Vertical speed for this lantern
            glm::vec3 p0{};
            glm::vec3 p1{};
            glm::vec3 p2{};
            glm::vec3 p3{};
            glm::vec3 position{};
            std::string meshName;
        };
        void updateLanterns(double deltaSeconds);
        void spawnLantern();
        glm::vec3 evaluateLanternPosition(const LanternInstance& lantern, float t) const;
        glm::vec3 evaluateLanternTangent(const LanternInstance& lantern, float t) const;
        void deactivateLantern(LanternInstance& lantern);
        std::optional<Mesh> m_lanternPrototype;
        std::vector<std::string> m_lanternMeshNames;
        std::vector<LanternInstance> m_lanternInstances;
        double m_lanternSpawnTimer{0.0};
        bool m_lanternPrototypeLoaded{false};
    };
} // namespace cg

