#include "util/MeshUtils.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace cg
{
    namespace MeshUtils
    {
        MeshBounds computeBounds(const Mesh& mesh)
        {
            MeshBounds bounds;
            for (const auto& vertex : mesh.vertices)
            {
                bounds.min = glm::min(bounds.min, vertex.position);
                bounds.max = glm::max(bounds.max, vertex.position);
            }
            return bounds;
        }

        void calculateNormals(Mesh& mesh)
        {
            if (mesh.vertices.size() < 3 || mesh.indices.size() < 3)
            {
                return;
            }

            // Initialize all normals to zero
            for (auto& vertex : mesh.vertices)
            {
                vertex.normal = glm::vec3(0.0f);
            }

            // Calculate face normals and accumulate to vertex normals
            for (size_t i = 0; i < mesh.indices.size(); i += 3)
            {
                if (i + 2 < mesh.indices.size())
                {
                    const uint32_t idx0 = mesh.indices[i];
                    const uint32_t idx1 = mesh.indices[i + 1];
                    const uint32_t idx2 = mesh.indices[i + 2];

                    const glm::vec3& v0 = mesh.vertices[idx0].position;
                    const glm::vec3& v1 = mesh.vertices[idx1].position;
                    const glm::vec3& v2 = mesh.vertices[idx2].position;

                    // Calculate face normal using cross product
                    const glm::vec3 edge1 = v1 - v0;
                    const glm::vec3 edge2 = v2 - v0;
                    glm::vec3 faceNormal = glm::cross(edge1, edge2);
                    const float length = glm::length(faceNormal);
                    if (length > 0.0001f)  // Avoid division by zero
                    {
                        faceNormal = faceNormal / length;

                        // Accumulate to vertex normals
                        mesh.vertices[idx0].normal += faceNormal;
                        mesh.vertices[idx1].normal += faceNormal;
                        mesh.vertices[idx2].normal += faceNormal;
                    }
                }
            }

            // Normalize all vertex normals
            for (auto& vertex : mesh.vertices)
            {
                const float length = glm::length(vertex.normal);
                if (length > 0.0001f)
                {
                    vertex.normal = vertex.normal / length;
                }
                else
                {
                    // Fallback to default normal if calculation failed
                    vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }
        }

        void transformZUpToYUp(Mesh& mesh)
        {
            for (auto& vertex : mesh.vertices)
            {
                vertex.position = glm::vec3(vertex.position.x, vertex.position.z, vertex.position.y);
                vertex.normal = glm::normalize(glm::vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y));
            }
        }
    }
} // namespace cg

