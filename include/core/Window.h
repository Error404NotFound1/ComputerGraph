#pragma once

#include "input/InputState.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <string_view>

namespace cg
{
    class Window
    {
    public:
        Window(int width, int height, std::string_view title, bool enableVSync);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool shouldClose() const;
        void swapBuffers();
        void pollEvents();
        void setInputState(InputState* inputState);
        GLFWwindow* rawHandle() const { return m_window; }

    private:
        static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        GLFWwindow* m_window{nullptr};
    };
} // namespace cg

