#include "core/Window.h"

#include "util/Log.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <stdexcept>

namespace
{
    constexpr const char* kInputStateKey = "cg_input_state";
}

namespace cg
{
    Window::Window(int width, int height, std::string_view title, bool enableVSync)
    {
        m_window = glfwCreateWindow(width, height, title.data(), nullptr, nullptr);
        if (!m_window)
        {
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(m_window);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        glfwSwapInterval(enableVSync ? 1 : 0);

        glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
        glfwSetCursorPosCallback(m_window, cursorPosCallback);
        glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
        glfwSetKeyCallback(m_window, keyCallback);
        glfwSetScrollCallback(m_window, scrollCallback);
    }

    Window::~Window()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    bool Window::shouldClose() const
    {
        return glfwWindowShouldClose(m_window);
    }

    void Window::swapBuffers()
    {
        glfwSwapBuffers(m_window);
    }

    void Window::pollEvents()
    {
        glfwPollEvents();
    }

    void Window::setInputState(InputState* inputState)
    {
        glfwSetWindowUserPointer(m_window, inputState);
        if (inputState)
        {
            inputState->cursorInitialized = false;
            inputState->cursorDelta = glm::vec2(0.0f);
        }
    }

    void Window::framebufferSizeCallback(GLFWwindow*, int width, int height)
    {
        glViewport(0, 0, width, height);
    }

    void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));
        if (!input) return;

        if (!input->freeLook) return;

        glm::vec2 current{static_cast<float>(xpos), static_cast<float>(ypos)};
        if (!input->cursorInitialized)
        {
            input->lastCursor = current;
            input->cursorInitialized = true;
            return;
        }

        input->cursorDelta += current - input->lastCursor;
        input->lastCursor = current;
    }

    void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int)
    {
        auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));
        if (!input) return;

        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            const bool pressed = (action == GLFW_PRESS);
            input->freeLook = pressed;
            glfwSetInputMode(window, GLFW_CURSOR, pressed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            input->cursorDelta = glm::vec2(0.0f);
            input->cursorInitialized = false;
        }
    }

    void Window::keyCallback(GLFWwindow* window, int key, int, int action, int)
    {
        auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));
        if (!input) return;

        const bool pressed = action != GLFW_RELEASE;
        switch (key)
        {
        case GLFW_KEY_W: input->forward = pressed; break;
        case GLFW_KEY_S: input->backward = pressed; break;
        case GLFW_KEY_A: input->left = pressed; break;
        case GLFW_KEY_D: input->right = pressed; break;
        case GLFW_KEY_Q: input->down = pressed; break;
        case GLFW_KEY_E: input->up = pressed; break;
        case GLFW_KEY_LEFT_SHIFT:
        case GLFW_KEY_RIGHT_SHIFT: input->boost = pressed; break;
        case GLFW_KEY_ESCAPE: if (pressed) glfwSetWindowShouldClose(window, GLFW_TRUE); break;
        default: break;
        }
    }

    void Window::scrollCallback(GLFWwindow* window, double, double yoffset)
    {
        auto* input = static_cast<InputState*>(glfwGetWindowUserPointer(window));
        if (!input) return;
        input->scrollDelta += static_cast<float>(yoffset);
    }
} // namespace cg

