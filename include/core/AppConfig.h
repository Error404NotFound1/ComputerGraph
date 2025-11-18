#pragma once

#include <string>
#include <array>
#include <glm/glm.hpp>

namespace cg
{
    struct AppConfig
    {
        std::string windowTitle{"SkyCity Scene"};
        int windowWidth{1920};
        int windowHeight{1080};
        bool enableVSync{true};
        
        // Anti-aliasing (MSAA) config
        int msaaSamples{4};
        
        // Texture quality config
        float textureAnisotropyLevel{16.0f};
        float textureLodBias{0.0f};
        
        // Distance-based texture quality adjustment
        float textureQualityNearDistance{5000.0f};
        float textureQualityFarDistance{25000.0f};
        float textureQualityMinFactor{0.3f};
        std::string daySkyboxPath{"models/skybox/day2.exr"};
        std::string nightSkyboxPath{"models/skybox/night2_8k.exr"};
        std::string groundMeshPath{"models/ground/groundtail"};
        float skyCycleDurationSeconds{30.0f};
        float daySkyboxYOffset{-0.0f};
        float nightSkyboxYOffset{-0.7f};
        float nightSkyboxBrightness{0.5f};
        
        // Skybox cycle config
        float skyDayDuration{15.0f};
        float skyDayToNightTransition{16.0f};
        float skyNightDuration{10.0f};
        float skyNightToDayTransition{10000.0f};
        int groundTilesPerSide{150};

        // Missile (rocket) config
        std::string missileModelPath{"models/plane/rocket/rocket.obj"};
        float missileDropTime{6.0f};
        float missileFallSpeed{700.0f};
        float missileFallAngle{60.0f};
        glm::vec3 missileScale{0.08f, 0.08f, 0.08f};
        float groundHeight{6000.0f};
        float missileRotationSpeed{180.0f};
        float missileCameraTrackDelay{2.0f};
        float missileCameraDistance{200.0f};
        float missileCameraHeight{50.0f};
        float missileCameraLookAhead{50.0f};
        bool enableMissileExplosion{true};
        int missileExplosionParticleCount{420};
        float missileExplosionMinSpeed{550.0f};
        float missileExplosionMaxSpeed{1400.0f};
        float missileExplosionMinLifetime{6.1f};
        float missileExplosionMaxLifetime{8.6f};
        float missileExplosionStartSize{280.0f};
        float missileExplosionEndSize{30.0f};
        float missileExplosionGravity{220.0f};
        std::array<glm::vec4, 6> missileExplosionColors{
            glm::vec4(1.0f, 0.35f, 0.35f, 1.0f),
            glm::vec4(1.0f, 0.7f, 0.2f, 1.0f),
            glm::vec4(0.95f, 0.95f, 0.25f, 1.0f),
            glm::vec4(0.4f, 0.9f, 0.5f, 1.0f),
            glm::vec4(0.35f, 0.6f, 1.0f, 1.0f),
            glm::vec4(0.75f, 0.4f, 1.0f, 1.0f)
        };

        // Material config
        bool enableFlagpoleMetalMaterial{true};
        bool enableMissileMetalMaterial{true};
        bool enableGroundProceduralMapping{true};
        bool enableFlagClothAnisotropy{true};
        
        // Kongming lantern config
        bool enableLanterns{true};
        std::string lanternModelPath{"models/kongming/OBJ/kongming.obj"};
        int lanternPoolSize{120};
        glm::vec3 lanternSpawnCenter{0.0f, 0.0f, -5000.0f };  // Center for horizontal spawn area
        glm::vec3 lanternSpawnHalfExtents{5000.0f, 0.0f, 5000.0f};  // Horizontal spawn area
        float lanternSpawnInterval{1.1f};
        float lanternSpawnStartTime{25.0f};
        glm::ivec2 lanternSpawnCountRange{5, 8};
        float lanternMinLifetime{20.0f};
        float lanternMaxLifetime{50.0f};
        float lanternTargetHeightMin{4000.0f};
        float lanternTargetHeightMax{99000.0f};
        float lanternMinSpeed{50.0f};  // Minimum vertical speed (units per second)
        float lanternMaxSpeed{150.0f};  // Maximum vertical speed (units per second)
        glm::vec3 lanternScale{0.25f, 0.25f, 0.25f };
        glm::vec3 lanternLightColor{1.0f, 0.7f, 0.4f};
        float lanternLightIntensity{25.0f};
        float lanternLightRadius{1500.0f};

        // Airplane animation config
        std::string airplaneModelPath{"models/plane/SR71.obj"};
        float airplaneSpawnTime{16.0f};
        float airplaneHeight{8000.0f};
        float airplaneSpeed{3000.0f};
        float airplaneLifetime{15.0f};
        glm::vec3 airplaneDirection{1.0f, 0.0f, 0.0f};
        glm::vec3 airplaneStartPosition{-20000.0f, 0.0f, -8000.0f};
        glm::vec3 airplaneScale{30.0f, 30.0f, 30.0f};
        
        // Wingman config
        std::string wingmanModelPath{"models/plane/SR71.obj"};
        glm::vec3 wingmanScale{30.0f, 30.0f, 30.0f};
        glm::vec3 wingmanLeft1Offset{-600.0f, 0.0f, -600.0f};
        glm::vec3 wingmanLeft2Offset{-1200.0f, 0.0f, -1200.0f};
        glm::vec3 wingmanRight1Offset{600.0f, 0.0f, -600.0f};
        glm::vec3 wingmanRight2Offset{1200.0f, 0.0f, -1200.0f};
        
        bool enableAirplaneCameraTracking{true};
        float airplaneCameraDistance{2000.0f};
        float airplaneCameraHeight{600.0f};
        bool airplaneCameraFollowPosition{false};
        
        // Airplane trail particle config
        bool enableAirplaneTrails{true};
        float airplaneTrailSpawnRate{40.0f};
        float airplaneTrailParticleLifetime{3.2f};
        float airplaneTrailStartSize{60.0f};
        float airplaneTrailEndSize{40.0f};
        float airplaneTrailInitialSpeed{450.0f};
        float airplaneTrailSpeedVariance{120.0f};
        float airplaneTrailEmissionOffset{250.0f};
        float airplaneTrailHorizontalJitter{60.0f};
        float airplaneTrailVerticalJitter{40.0f};
        float airplaneTrailLateralDrift{80.0f};
        float airplaneTrailVerticalDrift{40.0f};
        float airplaneTrailGravity{150.0f};
        int airplaneTrailMaxParticles{3000};
        std::array<glm::vec4, 5> airplaneTrailRainbowColors{
            glm::vec4(1.0f, 0.25f, 0.25f, 0.92f),
            glm::vec4(1.0f, 0.6f, 0.1f, 0.9f),
            glm::vec4(1.0f, 0.95f, 0.25f, 0.9f),
            glm::vec4(0.3f, 0.9f, 0.4f, 0.88f),
            glm::vec4(0.25f, 0.45f, 1.0f, 0.9f)
        };

        // Default camera config
        glm::vec3 defaultCameraPosition{0.0f, 1500.0f, 1500.0f};
        glm::vec3 defaultCameraTarget{0.0f, 0.0f, 0.0f};
        float defaultFOV{45.0f};  // Default field of view in degrees
        float finalFOV{75.0f};  // Final field of view when moving to keyframe 5 (wider angle)
        
        // Camera motion design (6 keyframes)
        bool enableCameraMotion{true};  // Enable/disable camera motion system
        struct CameraKeyframe
        {
            glm::vec3 position{0.0f, 0.0f, 0.0f};
            float yaw{0.0f};
            float pitch{0.0f};
        };
        std::array<CameraKeyframe, 6> cameraKeyframes{
            CameraKeyframe{{40.3, 1731.3, 6482.7}, -91.4f, -25.6f},  // Keyframe 0
            CameraKeyframe{{-189.2, 155.6, 5171.6}, -93.0f, 2.6f},   // Keyframe 1
            CameraKeyframe{{-241.2, 142.7, 3635.2}, -121.2f, 5.5f}, // Keyframe 2
            CameraKeyframe{{-106.8, 148.5, 2867.0}, -44.2f, 4.3f},   // Keyframe 3 (new position before original keyframe 3)
            CameraKeyframe{{37.3, 78.0, 1148.3}, -100.6f, 14.4f},   // Keyframe 4 (original keyframe 3)
            CameraKeyframe{{-13.2, 152.4, 398.7}, -102.2, 33.7f}     // Keyframe 5 (original keyframe 4)
        };
        // Transition times between keyframes (5 transitions for 6 keyframes)
        std::array<float, 5> cameraTransitionTimes{5.0f, 2.0f, 5.0f, 3.0f, 8.0f};  // Time to transition from keyframe i to i+1
        
        // Time display config
        bool showTimeDisplay{false};
        float timeDisplayScale{2.0f};
        
        // Ancient City model config
        std::string ancientCityModelPath{"models/gugong/ancientCity.obj"};
        glm::vec3 ancientCityPosition{0.0f, 30.0f, 0.0f};
        glm::vec3 ancientCityScale{100.0f, 100.0f, 100.0f};
        float ancientCityRotationY{-90.0f};
        bool enableAncientCity{true};
        
        // Flagpole config
        glm::vec3 flagpolePosition{0.0f, 0.0f, 0.0f};
        float flagpoleHeight{400.0f};
        float flagpoleRadius{3.0f};
        float flagpoleBallRadius{6.0f};
        int flagpoleSegments{16};
        glm::vec3 flagpoleColor{0.7f, 0.7f, 0.7f};
        glm::vec3 flagpoleBallColor{1.0f, 0.9f, 0.8f};
        bool enableFlagpole{true};
        
        // Flag config
        bool enableFlag{true};
        float flagWidth{120.0f};
        float flagHeight{80.0f};
        int flagControlPointsU{8};
        int flagControlPointsV{6};
        int flagSegmentsU{20};
        int flagSegmentsV{15};
        float flagWaveAmplitude{20.0f};
        float flagWaveFrequency{1.5f};
        std::string flagTexturePath{"models/FlagPole/image.png"};
        bool debugShowFlagControlPoints{false};
    };
}
