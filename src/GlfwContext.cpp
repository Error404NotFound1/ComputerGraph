#include "core/GlfwContext.h"

#include "util/Log.h"

namespace cg
{
    GlfwContext::GlfwContext()
    {
        if (!glfwInit())
        {
            log(LogLevel::Error, "Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        m_initialized = true;
    }

    void GlfwContext::setMSAASamples(int samples)
    {
        if (!m_initialized)
        {
            return;
        }

        // Set MSAA samples hint (must be called before window creation)
        glfwWindowHint(GLFW_SAMPLES, samples);
        m_msaaSamples = samples;
    }

    GlfwContext::~GlfwContext()
    {
        if (m_initialized)
        {
            glfwTerminate();
        }
    }
} // namespace cg

