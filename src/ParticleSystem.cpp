#include "render/ParticleSystem.h"

#include <glad/glad.h>

#include <algorithm>
#include <future>

namespace cg
{
    namespace
    {
        constexpr GLuint kParticlePosLocation = 0;
        constexpr GLuint kParticleColorLocation = 1;
        constexpr GLuint kParticleSizeLocation = 2;
    }

    ParticleSystem::ParticleSystem(size_t maxParticles)
        : m_maxParticles(maxParticles)
    {
        m_shader = std::make_unique<Shader>("shaders/particle.vert", "shaders/particle.frag");

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, m_maxParticles * sizeof(GpuParticle), nullptr, GL_STREAM_DRAW);

        glEnableVertexAttribArray(kParticlePosLocation);
        glVertexAttribPointer(kParticlePosLocation, 3, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), reinterpret_cast<void*>(offsetof(GpuParticle, position)));

        glEnableVertexAttribArray(kParticleColorLocation);
        glVertexAttribPointer(kParticleColorLocation, 4, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), reinterpret_cast<void*>(offsetof(GpuParticle, color)));

        glEnableVertexAttribArray(kParticleSizeLocation);
        glVertexAttribPointer(kParticleSizeLocation, 1, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), reinterpret_cast<void*>(offsetof(GpuParticle, size)));

        glBindVertexArray(0);
    }

    ParticleSystem::~ParticleSystem()
    {
        if (m_vbo != 0)
        {
            glDeleteBuffers(1, &m_vbo);
        }
        if (m_vao != 0)
        {
            glDeleteVertexArrays(1, &m_vao);
        }
    }

    void ParticleSystem::emit(const SpawnParams& params)
    {
        if (m_particles.size() >= m_maxParticles)
        {
            return;
        }

        Particle particle;
        particle.position = params.position;
        particle.velocity = params.velocity;
        particle.acceleration = params.acceleration;
        particle.baseColor = params.color;
        particle.startSize = params.startSize;
        particle.endSize = params.endSize;
        particle.lifetime = std::max(0.01f, params.lifetime);
        particle.age = 0.0f;
        particle.renderColor = params.color;
        particle.renderSize = params.startSize;

        m_particles.push_back(particle);
    }

    void ParticleSystem::update(float deltaSeconds)
    {
        if (m_particles.empty())
        {
            return;
        }

        const float dt = std::max(0.0f, deltaSeconds);
        auto processRange = [&](size_t begin, size_t end)
        {
            for (size_t i = begin; i < end; ++i)
            {
                Particle& particle = m_particles[i];
                particle.age += dt;
                particle.velocity += particle.acceleration * dt;
                particle.position += particle.velocity * dt;

                const float lifeRatio = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);
                const float alpha = std::clamp(1.0f - lifeRatio, 0.0f, 1.0f);
                particle.renderColor = glm::vec4(glm::vec3(particle.baseColor), particle.baseColor.a * alpha);
                particle.renderSize = glm::mix(particle.startSize, particle.endSize, lifeRatio);
            }
        };

        constexpr size_t kParallelThreshold = 1200;
        if (m_particles.size() > kParallelThreshold)
        {
            const size_t mid = m_particles.size() / 2;
            auto future = std::async(std::launch::async, processRange, mid, m_particles.size());
            processRange(0, mid);
            future.get();
        }
        else
        {
            processRange(0, m_particles.size());
        }

        m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
            [](const Particle& particle) { return particle.age >= particle.lifetime; }),
            m_particles.end());
    }

    void ParticleSystem::uploadParticlesToGpu()
    {
        m_gpuBuffer.clear();
        m_gpuBuffer.reserve(m_particles.size());

        for (const auto& particle : m_particles)
        {
            if (particle.renderColor.a <= 0.001f || particle.renderSize <= 0.0f)
            {
                continue;
            }

            GpuParticle gpuParticle;
            gpuParticle.position = particle.position;
            gpuParticle.color = particle.renderColor;
            gpuParticle.size = particle.renderSize;
            m_gpuBuffer.push_back(gpuParticle);
        }

        if (m_gpuBuffer.empty())
        {
            return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_gpuBuffer.size() * sizeof(GpuParticle), m_gpuBuffer.data());
    }

    void ParticleSystem::draw(const Camera& camera, float aspectRatio)
    {
        if (m_particles.empty())
        {
            return;
        }

        uploadParticlesToGpu();
        if (m_gpuBuffer.empty())
        {
            return;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_PROGRAM_POINT_SIZE);

        m_shader->bind();
        m_shader->setMat4("uView", camera.viewMatrix());
        m_shader->setMat4("uProj", camera.projectionMatrix(aspectRatio));

        glBindVertexArray(m_vao);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_gpuBuffer.size()));
        glBindVertexArray(0);

        glDisable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
} // namespace cg


