#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace cg
{
    class Camera
    {
    public:
        Camera();

        void rotate(float deltaYaw, float deltaPitch);
        void move(const glm::vec3& delta);
        void setPosition(const glm::vec3& position);
        void lookAt(const glm::vec3& target);
        void setRotation(float yaw, float pitch);

        glm::mat4 viewMatrix() const;
        glm::mat4 projectionMatrix(float aspect) const;
        glm::vec3 position() const;
        glm::vec3 forward() const;
        glm::vec3 right() const;
        glm::vec3 up() const;
        float yaw() const;
        float pitch() const;
        void setFOV(float fov);
        float fov() const;

    private:
        glm::vec3 m_position{0.0f, 500.0f, 500.0f};
        float m_yaw{-135.0f};
        float m_pitch{-20.0f};
        float m_fov{45.0f};  // Field of view in degrees
    };
} // namespace cg

