#include "core/App.h"

#include "util/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <filesystem>

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

        // Build base scene meshes
        auto meshes = buildDemoScene(m_config.groundMeshPath, m_config.groundTilesPerSide);

        // Optionally load rocket model and append to scene
        if (!m_config.rocketModelPath.empty())
        {
            if (auto rocket = loadObjAsMesh(m_config.rocketModelPath))
            {
                rocket->name = "rocket";
                // Set texture if there is a body texture next to the OBJ
                namespace fs = std::filesystem;
                fs::path objDir = fs::path(m_config.rocketModelPath).parent_path();
                fs::path bodyTex = objDir / "body.bmp";
                if (fs::exists(bodyTex))
                {
                    rocket->diffuseTexture = bodyTex.string();
                }

                // Compose transform from config
                glm::mat4 transform(1.0f);
                transform = glm::translate(transform, m_config.rocketPosition);
                transform = glm::rotate(transform, glm::radians(m_config.rocketYawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
                transform = glm::scale(transform, m_config.rocketScale);
                rocket->transform = transform;

                meshes.push_back(std::move(*rocket));
            }
        }

        m_scene = std::make_unique<Scene>(std::move(meshes));
        m_renderer = std::make_unique<SceneRenderer>(*m_scene);
        m_skybox = std::make_unique<SkyboxRenderer>();
        if (!m_skybox->loadEquirectangularTextures(m_config.daySkyboxPath, m_config.nightSkyboxPath))
        {
            log(LogLevel::Error, "Failed to load skybox textures.");
            return EXIT_FAILURE;
        }
		m_skybox->setNightBrightness(m_config.nightSkyboxBrightness);

        if (!m_scene->empty())
        {
            const glm::vec3 focus{0.0f, 200.0f, 0.0f};
            m_camera.setPosition(focus + glm::vec3(-1200.0f, 550.0f, 1200.0f));
            m_camera.lookAt(focus);
        }

        double deltaSeconds = 0.0;
        int framebufferWidth = m_config.windowWidth;
        int framebufferHeight = m_config.windowHeight;
        while (!m_window->shouldClose())
        {
            deltaSeconds = m_timer.tick();
            processInput(deltaSeconds);
            updateSkyBlend(deltaSeconds);
            m_window->pollEvents();
            glfwGetFramebufferSize(m_window->rawHandle(), &framebufferWidth, &framebufferHeight);

            glEnable(GL_DEPTH_TEST);
            glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const float aspect = framebufferHeight > 0 ? static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight) : 1.0f;
            m_skybox->draw(m_camera, aspect, m_skyBlend, m_config.daySkyboxYOffset, m_config.nightSkyboxYOffset);
            m_renderer->setEnvironmentBlend(m_skyBlend);
            m_renderer->draw(m_camera, aspect);

            m_window->swapBuffers();
        }

        return EXIT_SUCCESS;
    }

    void App::processInput(double deltaSeconds)
    {
        constexpr float rotationSensitivity = 0.08f;

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
        if (m_config.skyCycleDurationSeconds <= 0.0f)
        {
            m_skyBlend = 0.0f;
            return;
        }

        m_skyTime += deltaSeconds;
        const double duration = static_cast<double>(m_config.skyCycleDurationSeconds);
        if (duration > 0.0)
        {
            const double normalized = std::clamp(m_skyTime / duration, 0.0, 1.0);
            m_skyBlend = static_cast<float>(normalized);
        }
        else
        {
            m_skyBlend = 0.0f;
        }
    }
} // namespace cg

