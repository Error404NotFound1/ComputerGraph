#pragma once

#include "math/Camera.h"
#include "render/Shader.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace cg
{
    class ParticleSystem
    {
    public:
        struct SpawnParams
        {
            glm::vec3 position{0.0f};
            glm::vec3 velocity{0.0f};
            glm::vec3 acceleration{0.0f};
            glm::vec4 color{1.0f};
            float startSize{40.0f};
            float endSize{5.0f};
            float lifetime{1.0f};
        };

        explicit ParticleSystem(size_t maxParticles = 2000);
        ~ParticleSystem();

        ParticleSystem(const ParticleSystem&) = delete;
        ParticleSystem& operator=(const ParticleSystem&) = delete;

        void emit(const SpawnParams& params);
        void update(float deltaSeconds);
        void draw(const Camera& camera, float aspectRatio);

        size_t activeParticleCount() const { return m_particles.size(); }

    private:
        struct Particle
        {
            glm::vec3 position;
            glm::vec3 velocity;
            glm::vec3 acceleration;
            glm::vec4 baseColor;
            float startSize;
            float endSize;
            float lifetime;
            float age;
            glm::vec4 renderColor;
            float renderSize;
        };

        struct GpuParticle
        {
            glm::vec3 position;
            glm::vec4 color;
            float size;
        };

        void uploadParticlesToGpu();

        std::vector<Particle> m_particles;
        std::vector<GpuParticle> m_gpuBuffer;
        size_t m_maxParticles;

        GLuint m_vao{0};
        GLuint m_vbo{0};
        std::unique_ptr<Shader> m_shader;
    };
} // namespace cg


