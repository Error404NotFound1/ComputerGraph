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
        void setMSAASamples(int samples);  // Set MSAA samples (must be called before window creation)
        int getMSAASamples() const { return m_msaaSamples; }

    private:
        bool m_initialized{false};
        int m_msaaSamples{0};
    };
} // namespace cg

