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

    GlfwContext::~GlfwContext()
    {
        if (m_initialized)
        {
            glfwTerminate();
        }
    }
} // namespace cg

