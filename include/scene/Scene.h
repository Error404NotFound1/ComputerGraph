#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace cg
{
    struct Vertex
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
        glm::vec3 color{1.0f};
    };

    struct Mesh
    {
        std::string name;
        glm::mat4 transform{1.0f};
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string diffuseTexture;
    };

    struct SceneBounds
    {
        glm::vec3 min{0.0f};
        glm::vec3 max{0.0f};
        glm::vec3 center() const { return (min + max) * 0.5f; }
        float radius() const { return glm::length(max - center()); }
    };

    class Scene
    {
    public:
        explicit Scene(std::vector<Mesh> meshes);
        const std::vector<Mesh>& meshes() const { return m_meshes; }
        bool empty() const { return m_meshes.empty(); }
        const SceneBounds& bounds() const { return m_bounds; }

    private:
        std::vector<Mesh> m_meshes;
        SceneBounds m_bounds{};
    };
} // namespace cg

