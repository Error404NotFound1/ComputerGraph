#pragma once

#include <glm/glm.hpp>
#include <string>

namespace cg
{
    class TextRenderer
    {
    public:
        TextRenderer();
        ~TextRenderer();

        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;

        void drawText(const std::string& text, float x, float y, float scale = 1.0f, const glm::vec3& color = glm::vec3(1.0f));

    private:
        void initialize();
        void renderChar(char c, float x, float y, float width, float height, float thickness, const glm::vec3& color);
        void renderLine(float x1, float y1, float x2, float y2, float thickness, const glm::vec3& color);

        unsigned m_vao{0};
        unsigned m_vbo{0};
        unsigned m_shaderProgram{0};
        bool m_initialized{false};
    };
} // namespace cg

