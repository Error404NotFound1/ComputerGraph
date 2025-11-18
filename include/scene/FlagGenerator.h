#pragma once

#include "scene/Scene.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace cg
{
    namespace FlagGenerator
    {
        // Generate a Bezier surface flag mesh for waving animation
        Mesh generateBezierFlag(float width, float height, int controlPointsU, int controlPointsV, 
                                 int segmentsU, int segmentsV);
        
        // Update flag vertices with waving animation using control point displacement
        std::vector<Vertex> updateFlagVertices(
            float width, float height, int controlPointsU, int controlPointsV,
            int segmentsU, int segmentsV, float animationTime, float waveAmplitude, float waveFrequency,
            std::vector<glm::vec3>* outControlPoints = nullptr);
        
        // Generate a procedural flagpole (cylinder pole with sphere on top)
        std::vector<Mesh> generateFlagpole(float height, float poleRadius, float ballRadius, 
                                            int segments, const glm::vec3& poleColor, const glm::vec3& ballColor);

        // Generate debug mesh for visualizing flag control points
        Mesh generateFlagControlPointDebugMesh(
            float width, float height, int controlPointsU, int controlPointsV,
            float markerSize, const glm::vec3& color);

        // Update debug mesh vertices to match animated control point positions
        void updateFlagControlPointDebugVertices(
            const std::vector<glm::vec3>& controlPoints, float markerSize, const glm::vec3& color,
            std::vector<Vertex>& outVertices);
    }
} // namespace cg

