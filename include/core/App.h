#pragma once

#include "core/AppConfig.h"
#include "core/GlfwContext.h"
#include "core/Timer.h"
#include "core/Window.h"
#include "input/InputState.h"
#include "math/Camera.h"
#include "render/SceneRenderer.h"
#include "render/SkyboxRenderer.h"
#include "scene/DemoSceneBuilder.h"
#include "scene/Scene.h"

#include <memory>

namespace cg
{
    class App
    {
    public:
        explicit App(AppConfig config);
        int run();

    private:
        void processInput(double deltaSeconds);
        void updateSkyBlend(double deltaSeconds);

        AppConfig m_config;
        GlfwContext m_glfwContext;
        std::unique_ptr<Window> m_window;
        InputState m_input{};
        std::unique_ptr<Scene> m_scene;
        std::unique_ptr<SceneRenderer> m_renderer;
        std::unique_ptr<SkyboxRenderer> m_skybox;
        Timer m_timer;
        Camera m_camera;
        float m_moveSpeed{600.0f};
        double m_skyTime{0.0};
        float m_skyBlend{0.0f};
    };
} // namespace cg

