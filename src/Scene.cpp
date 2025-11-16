#include "scene/Scene.h"

#include <glm/glm.hpp>
#include <limits>

namespace cg
{
Scene::Scene(std::vector<Mesh> meshes)
    : m_meshes(std::move(meshes))
{
    if (m_meshes.empty())
    {
        m_bounds = SceneBounds{};
        return;
    }

    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

    for (const auto& mesh : m_meshes)
    {
        for (const auto& vertex : mesh.vertices)
        {
            const glm::vec4 worldPos = mesh.transform * glm::vec4(vertex.position, 1.0f);
            minBounds = glm::min(minBounds, glm::vec3(worldPos));
            maxBounds = glm::max(maxBounds, glm::vec3(worldPos));
        }
    }

    m_bounds.min = minBounds;
    m_bounds.max = maxBounds;
}
} // namespace cg

