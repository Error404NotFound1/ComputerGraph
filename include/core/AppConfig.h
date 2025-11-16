#pragma once

#include <string>
#include <glm/glm.hpp>

namespace cg
{
    struct AppConfig
    {
        std::string windowTitle{"SkyCity Scene"};
        int windowWidth{1920};
        int windowHeight{1080};
        bool enableVSync{true};
        std::string daySkyboxPath{"models/skybox/day2.exr"};
        std::string nightSkyboxPath{"models/skybox/night2_8k.exr"};
        std::string groundMeshPath{"models/ground/groundtail"};
        float skyCycleDurationSeconds{30.0f};
        float daySkyboxYOffset{-0.0f};
        float nightSkyboxYOffset{-0.7f};
		float nightSkyboxBrightness{0.5f};
        int groundTilesPerSide{100};

        // Rocket (missile) model config
        std::string rocketModelPath{"models/plane/rocket/rocket.obj"};
        glm::vec3 rocketPosition{0.0f, 30.0f, 0.0f};
        glm::vec3 rocketScale{0.1f, 0.1f, 0.1f};
        float rocketYawDegrees{0.0f};
    };
} // namespace cg

