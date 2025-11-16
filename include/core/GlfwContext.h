#pragma once

#include <glad/glad.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace cg
{
    class GlfwContext
    {
    public:
        GlfwContext();
        ~GlfwContext();

        GlfwContext(const GlfwContext&) = delete;
        GlfwContext& operator=(const GlfwContext&) = delete;

        bool isInitialized() const noexcept { return m_initialized; }

    private:
        bool m_initialized{false};
    };
} // namespace cg

