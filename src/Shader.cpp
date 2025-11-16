#include "render/Shader.h"

#include "util/FileSystem.h"
#include "util/Log.h"

#include <stdexcept>

namespace cg
{
    namespace
    {
        GLuint compile(GLenum type, const std::string& source)
        {
            GLuint shader = glCreateShader(type);
            const char* data = source.c_str();
            glShaderSource(shader, 1, &data, nullptr);
            glCompileShader(shader);

            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                GLint length = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                std::string log(length, '\0');
                glGetShaderInfoLog(shader, length, nullptr, log.data());
                throw std::runtime_error("Shader compilation failed: " + log);
            }

            return shader;
        }
    }

    Shader::Shader(std::string_view vertexPath, std::string_view fragmentPath)
    {
        const std::string vertexSource = readTextFile(std::string(vertexPath));
        const std::string fragmentSource = readTextFile(std::string(fragmentPath));

        GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint success;
        glGetProgramiv(m_program, GL_LINK_STATUS, &success);
        if (!success)
        {
            GLint length = 0;
            glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
            std::string logStr(length, '\0');
            glGetProgramInfoLog(m_program, length, nullptr, logStr.data());
            throw std::runtime_error("Shader linkage failed: " + logStr);
        }
    }

    Shader::~Shader()
    {
        if (m_program != 0)
        {
            glDeleteProgram(m_program);
        }
    }

    void Shader::bind() const
    {
        glUseProgram(m_program);
    }

    void Shader::setMat4(std::string_view name, const glm::mat4& value) const
    {
        const GLint location = glGetUniformLocation(m_program, name.data());
        glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
    }

    void Shader::setVec3(std::string_view name, const glm::vec3& value) const
    {
        const GLint location = glGetUniformLocation(m_program, name.data());
        glUniform3fv(location, 1, &value[0]);
    }

    void Shader::setFloat(std::string_view name, float value) const
    {
        const GLint location = glGetUniformLocation(m_program, name.data());
        glUniform1f(location, value);
    }

    void Shader::setInt(std::string_view name, int value) const
    {
        const GLint location = glGetUniformLocation(m_program, name.data());
        glUniform1i(location, value);
    }
} // namespace cg

