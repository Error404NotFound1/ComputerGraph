#pragma once

#include "math/Camera.h"
#include "render/Shader.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace cg
{
    class SkyboxRenderer
    {
    public:
        SkyboxRenderer();
        ~SkyboxRenderer();

        SkyboxRenderer(const SkyboxRenderer&) = delete;
        SkyboxRenderer& operator=(const SkyboxRenderer&) = delete;

		bool loadEquirectangularTextures(const std::string& dayPath, const std::string& nightPath);
		void draw(const Camera& camera, float aspectRatio, float blend, float dayYOffset, float nightYOffset);
		void setNightBrightness(float brightness) { m_nightBrightness = brightness; }

    private:
        std::unique_ptr<Shader> m_shader;
        unsigned m_vao{0};
        unsigned m_vbo{0};
        unsigned m_dayTexture{0};
        unsigned m_nightTexture{0};
		float m_nightBrightness{1.0f};

        unsigned loadTexture(const std::string& path) const;
        void createCubeGeometry();
    };
} // namespace cg


