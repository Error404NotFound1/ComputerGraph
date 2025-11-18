#include "render/TextRenderer.h"

#include <glad/glad.h>
#include "util/Log.h"

#include <sstream>
#include <iomanip>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace cg
{
    namespace
    {
        // Simple shader for text rendering (screen-space)
        const char* vertexShaderSource = R"(
#version 450 core
layout (location = 0) in vec2 aPos;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

        const char* fragmentShaderSource = R"(
#version 450 core
out vec4 FragColor;

uniform vec3 uTextColor;

void main()
{
    FragColor = vec4(uTextColor, 1.0);
}
)";

        GLuint compileShader(GLenum type, const char* source)
        {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                GLint length = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                std::string logStr(length, '\0');
                glGetShaderInfoLog(shader, length, nullptr, logStr.data());
                log(LogLevel::Error, "Text shader compilation failed: " + logStr);
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        GLuint createShaderProgram()
        {
            GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
            GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

            if (vertexShader == 0 || fragmentShader == 0)
            {
                if (vertexShader != 0) glDeleteShader(vertexShader);
                if (fragmentShader != 0) glDeleteShader(fragmentShader);
                return 0;
            }

            GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);
            glLinkProgram(program);

            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            GLint success;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (!success)
            {
                GLint length = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                std::string logStr(length, '\0');
                glGetProgramInfoLog(program, length, nullptr, logStr.data());
                log(LogLevel::Error, "Text shader linkage failed: " + logStr);
                glDeleteProgram(program);
                return 0;
            }

            return program;
        }
    }

    TextRenderer::TextRenderer()
    {
        initialize();
    }

    TextRenderer::~TextRenderer()
    {
        if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
        if (m_shaderProgram != 0) glDeleteProgram(m_shaderProgram);
    }

    void TextRenderer::initialize()
    {
        if (m_initialized) return;

        m_shaderProgram = createShaderProgram();
        if (m_shaderProgram == 0)
        {
            log(LogLevel::Error, "Failed to create text shader program");
            return;
        }

        // Create a buffer for dynamic line rendering (only positions, no texCoords)
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        // Allocate buffer for up to 6 vertices (a quad/triangle strip) - will be updated dynamically
        glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        // Only position attribute (2 floats per vertex)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        
        // Disable texCoord attribute since we don't use it
        glDisableVertexAttribArray(1);

        glBindVertexArray(0);
        m_initialized = true;
    }

    void TextRenderer::drawText(const std::string& text, float x, float y, float scale, const glm::vec3& color)
    {
        if (!m_initialized || m_shaderProgram == 0) return;

        // Save current OpenGL state
        GLint prevProgram, prevVAO, prevArrayBuffer;
        GLint prevViewport[4];
        GLboolean prevBlend, prevDepthTest, prevCullFace;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        prevBlend = glIsEnabled(GL_BLEND);
        prevDepthTest = glIsEnabled(GL_DEPTH_TEST);
        prevCullFace = glIsEnabled(GL_CULL_FACE);

        // Setup for 2D text rendering
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Disable multisampling for clearer lines (if enabled)
        glDisable(GL_MULTISAMPLE);

        glUseProgram(m_shaderProgram);
        glBindVertexArray(m_vao);

        // Set text color
        GLint colorLoc = glGetUniformLocation(m_shaderProgram, "uTextColor");
        glUniform3fv(colorLoc, 1, &color[0]);

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        const float screenWidth = static_cast<float>(viewport[2] > 0 ? viewport[2] : 1920);
        const float screenHeight = static_cast<float>(viewport[3] > 0 ? viewport[3] : 1080);

        // Convert screen coordinates to NDC
        const float ndcX = (x / screenWidth) * 2.0f - 1.0f;
        const float ndcY = 1.0f - (y / screenHeight) * 2.0f;
        
        // Character dimensions in pixels, then convert to NDC
        const float charWidthPx = 16.0f * scale;
        const float charHeightPx = 24.0f * scale;
        const float charSpacingPx = 4.0f * scale;
        
        const float charWidthNdc = (charWidthPx / screenWidth) * 2.0f;
        const float charHeightNdc = (charHeightPx / screenHeight) * 2.0f;
        const float charSpacingNdc = (charSpacingPx / screenWidth) * 2.0f;

        float currentX = ndcX;
        // Increase line thickness for better visibility
        const float lineThickness = (4.0f * scale / screenWidth) * 2.0f;

        // Render each character using line segments (7-segment style for simplicity)
        for (char c : text)
        {
            if (c == '\n')
            {
                currentX = ndcX;
                continue;
            }

            // Simple digit rendering using lines
            renderChar(c, currentX, ndcY, charWidthNdc, charHeightNdc, lineThickness, color);

            currentX += charWidthNdc + charSpacingNdc;
        }

        // Restore OpenGL state
        glBindVertexArray(prevVAO);
        glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
        glUseProgram(prevProgram);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        if (!prevBlend) glDisable(GL_BLEND);
        if (prevDepthTest) glEnable(GL_DEPTH_TEST);
        if (prevCullFace) glEnable(GL_CULL_FACE);
    }

    void TextRenderer::renderLine(float x1, float y1, float x2, float y2, float thickness, const glm::vec3& color)
    {
        // Calculate perpendicular direction for line thickness
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.001f) return;
        
        dx /= len;
        dy /= len;
        
        // Perpendicular vector
        float perpX = -dy;
        float perpY = dx;
        
        float halfThick = thickness * 0.5f;
        
        // Create a quad for the line
        float vertices[] = {
            x1 + perpX * halfThick, y1 + perpY * halfThick,
            x1 - perpX * halfThick, y1 - perpY * halfThick,
            x2 - perpX * halfThick, y2 - perpY * halfThick,
            x1 + perpX * halfThick, y1 + perpY * halfThick,
            x2 - perpX * halfThick, y2 - perpY * halfThick,
            x2 + perpX * halfThick, y2 + perpY * halfThick
        };

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void TextRenderer::renderChar(char c, float x, float y, float width, float height, float thickness, const glm::vec3& color)
    {
        // Simple 7-segment style rendering for digits and common characters
        // x, y are top-left corner in NDC (y is at top of screen, decreases downward)
        const float w = width * 0.75f;  // Slightly wider characters
        const float h = height * 0.8f;   // Slightly taller characters
        const float cx = x + width * 0.5f;  // Center X of character (use full width)
        const float cy = y - h * 0.5f;  // Center Y of character (y is top in NDC, so center is y - h/2, since y decreases downward)
        const float halfW = w * 0.5f;
        const float halfH = h * 0.5f;

        switch (c)
        {
        case '0':
            renderLine(cx - halfW, cy - halfH, cx - halfW, cy + halfH, thickness, color);
            renderLine(cx + halfW, cy - halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '1':
            renderLine(cx, cy - halfH, cx, cy + halfH, thickness, color);
            break;
        case '2':
            // Top horizontal line
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            // Right vertical line (top half)
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy, thickness, color);
            // Middle horizontal line
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            // Left vertical line (bottom half)
            renderLine(cx - halfW, cy, cx - halfW, cy - halfH, thickness, color);
            // Bottom horizontal line
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '3':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '4':
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '5':
            renderLine(cx + halfW, cy + halfH, cx - halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '6':
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '7':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '8':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy - halfH, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case '9':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case ':':
            // Colon (two dots)
            renderLine(cx, cy - halfH * 0.3f, cx, cy - halfH * 0.1f, thickness * 2.0f, color);
            renderLine(cx, cy + halfH * 0.1f, cx, cy + halfH * 0.3f, thickness * 2.0f, color);
            break;
        case '.':
            renderLine(cx, cy - halfH, cx, cy - halfH + thickness * 2.0f, thickness * 2.0f, color);
            break;
        case '-':
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            break;
        case 'T':
        case 't':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx, cy + halfH, cx, cy - halfH, thickness, color);
            break;
        case 'i':
        case 'I':
            renderLine(cx, cy + halfH, cx, cy - halfH, thickness, color);
            break;
        case 'm':
        case 'M':
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy - halfH, thickness, color);
            renderLine(cx + halfW, cy + halfH, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx, cy, thickness, color);
            renderLine(cx, cy, cx + halfW, cy + halfH, thickness, color);
            break;
        case 'e':
        case 'E':
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        case 's':
        case 'S':
            renderLine(cx - halfW, cy + halfH, cx + halfW, cy + halfH, thickness, color);
            renderLine(cx - halfW, cy + halfH, cx - halfW, cy, thickness, color);
            renderLine(cx - halfW, cy, cx + halfW, cy, thickness, color);
            renderLine(cx + halfW, cy, cx + halfW, cy - halfH, thickness, color);
            renderLine(cx - halfW, cy - halfH, cx + halfW, cy - halfH, thickness, color);
            break;
        default:
            // Unknown character - skip
            break;
        }
    }
} // namespace cg

