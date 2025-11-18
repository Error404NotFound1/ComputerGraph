#include "math/Camera.h"

namespace cg
{
    namespace
    {
        constexpr glm::vec3 kWorldUp{0.0f, 1.0f, 0.0f};
    }

    Camera::Camera() = default;

    void Camera::rotate(float deltaYaw, float deltaPitch)
    {
        m_yaw += deltaYaw;
        m_pitch = glm::clamp(m_pitch + deltaPitch, -89.0f, 89.0f);
    }

    void Camera::move(const glm::vec3& delta)
    {
        m_position += delta;
    }

    void Camera::setPosition(const glm::vec3& position)
    {
        m_position = position;
    }

    void Camera::lookAt(const glm::vec3& target)
    {
        const glm::vec3 dir = glm::normalize(target - m_position);
        m_pitch = glm::degrees(asinf(dir.y));
        m_yaw = glm::degrees(atan2f(dir.z, dir.x));
    }

    void Camera::setRotation(float yaw, float pitch)
    {
        m_yaw = yaw;
        m_pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    glm::mat4 Camera::viewMatrix() const
    {
        return glm::lookAt(m_position, m_position + forward(), up());
    }

    glm::mat4 Camera::projectionMatrix(float aspect) const
    {
        return glm::perspective(glm::radians(m_fov), aspect, 0.1f, 30000.0f);
    }

    void Camera::setFOV(float fov)
    {
        m_fov = glm::clamp(fov, 10.0f, 120.0f);  // Clamp FOV to reasonable range
    }

    float Camera::fov() const
    {
        return m_fov;
    }

    glm::vec3 Camera::position() const
    {
        return m_position;
    }

    glm::vec3 Camera::forward() const
    {
        const float yawRad = glm::radians(m_yaw);
        const float pitchRad = glm::radians(m_pitch);

        glm::vec3 direction;
        direction.x = cosf(yawRad) * cosf(pitchRad);
        direction.y = sinf(pitchRad);
        direction.z = sinf(yawRad) * cosf(pitchRad);

        return glm::normalize(direction);
    }

    glm::vec3 Camera::right() const
    {
        return glm::normalize(glm::cross(forward(), kWorldUp));
    }

    glm::vec3 Camera::up() const
    {
        return glm::normalize(glm::cross(right(), forward()));
    }

    float Camera::yaw() const
    {
        return m_yaw;
    }

    float Camera::pitch() const
    {
        return m_pitch;
    }
} // namespace cg

