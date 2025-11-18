#include "render/SkyboxRenderer.h"

#include "util/Log.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <vector>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace cg
{
    namespace
    {
        constexpr float kCubeVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        constexpr int kVertexCount = 36;

        GLuint createTextureFromPixels(int width, int height, int channels, const float* data)
        {
            if (channels != 3 && channels != 4)
            {
                log(LogLevel::Error, "Unsupported channel count for skybox texture.");
                return 0;
            }

            const GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
            const GLint internalFormat = channels == 4 ? GL_RGBA16F : GL_RGB16F;

            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_FLOAT, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            return texture;
        }

        std::string toLower(std::string str)
        {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return str;
        }
    } // namespace

    SkyboxRenderer::SkyboxRenderer()
    {
        createCubeGeometry();
        m_shader = std::make_unique<Shader>("shaders/skybox.vert", "shaders/skybox.frag");
    }

    SkyboxRenderer::~SkyboxRenderer()
    {
        if (m_dayTexture != 0) glDeleteTextures(1, &m_dayTexture);
        if (m_nightTexture != 0) glDeleteTextures(1, &m_nightTexture);
        if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
        if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
    }

    bool SkyboxRenderer::loadEquirectangularTextures(const std::string& dayPath, const std::string& nightPath)
    {
        m_dayTexture = loadTexture(dayPath);
        m_nightTexture = loadTexture(nightPath);

        if (m_dayTexture == 0 || m_nightTexture == 0)
        {
            log(LogLevel::Error, "Failed to load skybox textures.");
            return false;
        }

        return true;
    }

	void SkyboxRenderer::draw(const Camera& camera, float aspectRatio, float blend, float dayYOffset, float nightYOffset)
    {
        if (m_dayTexture == 0 || m_nightTexture == 0)
        {
            return;
        }

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

    m_shader->bind();
    // Use only the rotation part of the camera view (no translation)
    glm::mat4 viewRotation = glm::mat4(glm::mat3(camera.viewMatrix()));
    // Rotate the whole skybox by 180 degrees around Y axis so the sky orientation flips
    const glm::mat4 skyRotation = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    m_shader->setMat4("uView", viewRotation * skyRotation);
		m_shader->setMat4("uProj", camera.projectionMatrix(aspectRatio));
		const float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
		m_shader->setFloat("uBlend", clampedBlend);
		const float skyYOffset = (1.0f - clampedBlend) * dayYOffset + clampedBlend * nightYOffset;
		m_shader->setFloat("uSkyYOffset", skyYOffset);
		m_shader->setFloat("uNightBrightness", m_nightBrightness);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_dayTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_nightTexture);

        m_shader->setInt("uDaySampler", 0);
        m_shader->setInt("uNightSampler", 1);

        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, kVertexCount);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    unsigned SkyboxRenderer::loadTexture(const std::string& path) const
    {
        const std::string extension = toLower(std::filesystem::path(path).extension().string());
        if (extension == ".exr")
        {
            float* exrData = nullptr;
            int width = 0;
            int height = 0;
            const char* err = nullptr;
            const int ret = LoadEXR(&exrData, &width, &height, path.c_str(), &err);
            if (ret != TINYEXR_SUCCESS)
            {
                std::string msg = "Failed to load EXR texture: " + path;
                if (err != nullptr)
                {
                    msg += " (" + std::string(err) + ")";
                    FreeEXRErrorMessage(err);
                }
                log(LogLevel::Error, msg);
                if (exrData) free(exrData);
                return 0;
            }

            std::vector<float> pixels(exrData, exrData + (width * height * 4));
            free(exrData);
            return createTextureFromPixels(width, height, 4, pixels.data());
        }

        stbi_set_flip_vertically_on_load(false);
        int width = 0;
        int height = 0;
        int channels = 0;
        float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
        if (!data)
        {
            log(LogLevel::Error, std::string("Failed to load HDR texture: ") + path + " (" + (stbi_failure_reason() ? stbi_failure_reason() : "unknown error") + ")");
            return 0;
        }

        if (channels < 3)
        {
            log(LogLevel::Error, "Skybox texture requires at least 3 channels: " + path);
            stbi_image_free(data);
            return 0;
        }

        const int usedChannels = channels >= 4 ? 4 : 3;
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * usedChannels;
        std::vector<float> pixels(pixelCount);
        for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); ++i)
        {
            for (int c = 0; c < usedChannels; ++c)
            {
                pixels[i * usedChannels + c] = data[i * channels + c];
            }
        }

        stbi_image_free(data);
        return createTextureFromPixels(width, height, usedChannels, pixels.data());
    }

    void SkyboxRenderer::createCubeGeometry()
    {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glBindVertexArray(0);
    }
} // namespace cg


