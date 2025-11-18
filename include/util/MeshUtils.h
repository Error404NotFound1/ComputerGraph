#pragma once

#include "scene/Scene.h"

#include <glm/glm.hpp>
#include <limits>

namespace cg
{
    namespace MeshUtils
    {
        // Compute bounding box for a mesh
        struct MeshBounds
        {
            glm::vec3 min{std::numeric_limits<float>::max()};
            glm::vec3 max{std::numeric_limits<float>::lowest()};
            glm::vec3 extent() const { return max - min; }
            glm::vec3 center() const { return (min + max) * 0.5f; }
        };

        MeshBounds computeBounds(const Mesh& mesh);
        
        // Calculate vertex normals from face normals
        void calculateNormals(Mesh& mesh);
        
        // Transform mesh from Z-up to Y-up coordinate system
        void transformZUpToYUp(Mesh& mesh);
    }
} // namespace cg

