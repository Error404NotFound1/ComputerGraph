#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <string_view>

namespace cg
{
    class Shader
    {
    public:
        Shader(std::string_view vertexPath, std::string_view fragmentPath);
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        void bind() const;
        void setMat4(std::string_view name, const glm::mat4& value) const;
        void setVec3(std::string_view name, const glm::vec3& value) const;
        void setFloat(std::string_view name, float value) const;
        void setInt(std::string_view name, int value) const;

    private:
        GLuint m_program{0};
    };
} // namespace cg

