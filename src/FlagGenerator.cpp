#include "scene/FlagGenerator.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <array>

namespace cg
{
    namespace FlagGenerator
    {
        // Calculate Bernstein basis function for Bezier curve of degree n
        // B_i^n(t) = C(n,i) * t^i * (1-t)^(n-i)
        inline float bernsteinBasis(int n, int i, float t)
        {
            if (i < 0 || i > n) return 0.0f;
            if (n == 0) return 1.0f;
            
            // Calculate binomial coefficient C(n,i) iteratively
            float binomial = 1.0f;
            for (int j = 0; j < i; ++j)
            {
                binomial = binomial * (n - j) / (j + 1);
            }
            
            // Calculate t^i * (1-t)^(n-i)
            float tPower = std::pow(t, static_cast<float>(i));
            float oneMinusTPower = std::pow(1.0f - t, static_cast<float>(n - i));
            
            return binomial * tPower * oneMinusTPower;
        }
        
        // Calculate derivative of Bernstein basis function
        inline float bernsteinBasisDerivative(int n, int i, float t)
        {
            if (i < 0 || i > n) return 0.0f;
            if (n == 0) return 0.0f;
            
            if (i == 0)
            {
                return -n * std::pow(1.0f - t, n - 1);
            }
            else if (i == n)
            {
                return n * std::pow(t, n - 1);
            }
            else
            {
                float result = 1.0f;
                for (int j = 0; j < i; ++j)
                {
                    result = result * (n - j) / (j + 1);
                }
                
                float term1 = i * std::pow(t, i - 1) * std::pow(1.0f - t, n - i);
                float term2 = (n - i) * std::pow(t, i) * std::pow(1.0f - t, n - i - 1);
                return result * (term1 - term2);
            }
        }
        
        namespace
        {
            inline float pseudoRandom(int i, int j, float seed)
            {
                float dotVal = static_cast<float>(i) * 12.9898f + static_cast<float>(j) * 78.233f + seed * 37.719f;
                float sinVal = std::sin(dotVal) * 43758.5453f;
                return sinVal - std::floor(sinVal);
            }
        }
        
        Mesh generateBezierFlag(float width, float height, int controlPointsU, int controlPointsV,
                                int segmentsU, int segmentsV)
        {
            Mesh flag;
            flag.name = "flag";
            
            // Clamp control points to minimum 2
            controlPointsU = std::max(2, controlPointsU);
            controlPointsV = std::max(2, controlPointsV);
            
            // Initialize control points grid (base positions, no animation)
            std::vector<std::vector<glm::vec3>> controlPoints(controlPointsU, std::vector<glm::vec3>(controlPointsV));
            
            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;
            
            // Initialize control points in a flat grid
            for (int i = 0; i < controlPointsU; ++i)
            {
                float u = static_cast<float>(i) / static_cast<float>(controlPointsU - 1);
                float x = glm::mix(-halfWidth, halfWidth, u);
                
                for (int j = 0; j < controlPointsV; ++j)
                {
                    float v = static_cast<float>(j) / static_cast<float>(controlPointsV - 1);
                    float y = glm::mix(-halfHeight, halfHeight, v);
                    
                    // Base position: flat plane
                    controlPoints[i][j] = glm::vec3(x, y, 0.0f);
                }
            }
            
            flag.vertices.reserve((segmentsU + 1) * (segmentsV + 1));
            flag.indices.reserve(segmentsU * segmentsV * 6);
            
            // Generate vertices using Bezier surface evaluation
            for (int v = 0; v <= segmentsV; ++v)
            {
                float vParam = static_cast<float>(v) / static_cast<float>(segmentsV);
                
                for (int u = 0; u <= segmentsU; ++u)
                {
                    float uParam = static_cast<float>(u) / static_cast<float>(segmentsU);
                    
                    // Evaluate Bezier surface at (uParam, vParam)
                    glm::vec3 position(0.0f);
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float basisU = bernsteinBasis(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float basisV = bernsteinBasis(controlPointsV - 1, j, vParam);
                            position += controlPoints[i][j] * basisU * basisV;
                        }
                    }
                    
                    // Calculate tangents for normal computation
                    glm::vec3 tangentU(0.0f);
                    glm::vec3 tangentV(0.0f);
                    
                    // Tangent in U direction
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float dBasisU = bernsteinBasisDerivative(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float basisV = bernsteinBasis(controlPointsV - 1, j, vParam);
                            tangentU += controlPoints[i][j] * dBasisU * basisV;
                        }
                    }
                    
                    // Tangent in V direction
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float basisU = bernsteinBasis(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float dBasisV = bernsteinBasisDerivative(controlPointsV - 1, j, vParam);
                            tangentV += controlPoints[i][j] * basisU * dBasisV;
                        }
                    }
                    
                    glm::vec3 normal = glm::normalize(glm::cross(tangentU, tangentV));
                    if (glm::length(normal) < 0.001f)
                    {
                        normal = glm::vec3(0.0f, 0.0f, 1.0f);
                    }
                    
                    Vertex vertex;
                    vertex.position = position;
                    vertex.normal = normal;
                    vertex.uv = glm::vec2(uParam, 1.0f - vParam);
                    vertex.color = glm::vec3(1.0f);
                    
                    flag.vertices.push_back(vertex);
                }
            }
            
            // Generate indices
            for (int v = 0; v < segmentsV; ++v)
            {
                for (int u = 0; u < segmentsU; ++u)
                {
                    uint32_t topLeft = static_cast<uint32_t>(v * (segmentsU + 1) + u);
                    uint32_t topRight = topLeft + 1;
                    uint32_t bottomLeft = static_cast<uint32_t>((v + 1) * (segmentsU + 1) + u);
                    uint32_t bottomRight = bottomLeft + 1;
                    
                    flag.indices.push_back(topLeft);
                    flag.indices.push_back(bottomLeft);
                    flag.indices.push_back(topRight);
                    
                    flag.indices.push_back(topRight);
                    flag.indices.push_back(bottomLeft);
                    flag.indices.push_back(bottomRight);
                }
            }
            
            flag.transform = glm::mat4(1.0f);
            return flag;
        }

        std::vector<Vertex> updateFlagVertices(
            float width, float height, int controlPointsU, int controlPointsV,
            int segmentsU, int segmentsV, float animationTime, float waveAmplitude, float waveFrequency,
            std::vector<glm::vec3>* outControlPoints)
        {
            std::vector<Vertex> vertices;
            vertices.reserve((segmentsU + 1) * (segmentsV + 1));
            
            // Clamp control points to minimum 2
            controlPointsU = std::max(2, controlPointsU);
            controlPointsV = std::max(2, controlPointsV);
            
            // Initialize control points grid
            std::vector<std::vector<glm::vec3>> controlPoints(controlPointsU, std::vector<glm::vec3>(controlPointsV));
            
            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;
            
            // Calculate wave phase
            float wavePhase = animationTime * waveFrequency * 2.0f * glm::pi<float>();
            
            if (outControlPoints != nullptr)
            {
                outControlPoints->clear();
                outControlPoints->reserve(controlPointsU * controlPointsV);
            }
            
            // Initialize control points with base positions and animated displacements
            for (int i = 0; i < controlPointsU; ++i)
            {
                float u = static_cast<float>(i) / static_cast<float>(controlPointsU - 1);
                float x = glm::mix(-halfWidth, halfWidth, u);
                
                // Calculate displacement factor based on distance from pole (left edge)
                // Left edge (i=0) is fixed, right edge has maximum displacement
                float displacementFactor = u * u;  // Quadratic easing for smoother transition
                displacementFactor = glm::mix(0.25f, 1.0f, displacementFactor);
                if (i == 0)
                {
                    displacementFactor = 0.0f;
                }
                
                for (int j = 0; j < controlPointsV; ++j)
                {
                    float v = static_cast<float>(j) / static_cast<float>(controlPointsV - 1);
                    float y = glm::mix(-halfHeight, halfHeight, v);
                    
                    // Base position
                    glm::vec3 basePos = glm::vec3(x, y, 0.0f);
                    
                    // Calculate wave displacement for this control point
                    // Use both vertical position (v) and horizontal position (u) for wave pattern
                    float randomPhase = pseudoRandom(i, j, 0.0f) * glm::two_pi<float>();
                    float randomAmplitudeScale = glm::mix(0.7f, 1.4f, pseudoRandom(i, j, 1.0f));
                    float randomFrequencyScale = glm::mix(0.8f, 1.6f, pseudoRandom(i, j, 2.0f));
                    float randomDrift = pseudoRandom(i, j, 3.0f);
                    
                    float baseWave = v * glm::two_pi<float>() + wavePhase * randomFrequencyScale + u * 2.0f + randomPhase;
                    float lateralNoise = std::sin(wavePhase * 0.35f + randomDrift * glm::two_pi<float>());
                    float verticalNoise = std::cos(wavePhase * 0.55f + randomDrift * glm::pi<float>());
                    
                    // Moderated displacement coefficients for subtler motion
                    float waveOffsetX = (std::sin(baseWave) + lateralNoise * 0.35f) * waveAmplitude * randomAmplitudeScale * displacementFactor * 1.1f;
                    float waveOffsetZ = (std::cos(baseWave) + verticalNoise * 0.3f) * waveAmplitude * randomAmplitudeScale * displacementFactor * 0.95f;
                    
                    // Add additional vertical wave component for more dynamic movement
                    float waveOffsetY = std::sin(v * glm::pi<float>() * 3.0f + wavePhase * 1.3f + u * 1.5f + randomPhase * 0.5f) *
                        waveAmplitude * 0.35f * randomAmplitudeScale * displacementFactor;
                    
                    // Apply displacement to control point
                    controlPoints[i][j] = basePos + glm::vec3(waveOffsetX, waveOffsetY, waveOffsetZ);
                    
                    if (outControlPoints != nullptr)
                    {
                        outControlPoints->push_back(controlPoints[i][j]);
                    }
                }
            }
            
            // Generate vertices using Bezier surface evaluation
            for (int v = 0; v <= segmentsV; ++v)
            {
                float vParam = static_cast<float>(v) / static_cast<float>(segmentsV);
                
                for (int u = 0; u <= segmentsU; ++u)
                {
                    float uParam = static_cast<float>(u) / static_cast<float>(segmentsU);
                    
                    // Evaluate Bezier surface at (uParam, vParam)
                    glm::vec3 position(0.0f);
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float basisU = bernsteinBasis(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float basisV = bernsteinBasis(controlPointsV - 1, j, vParam);
                            position += controlPoints[i][j] * basisU * basisV;
                        }
                    }
                    
                    // Calculate tangents for normal computation
                    glm::vec3 tangentU(0.0f);
                    glm::vec3 tangentV(0.0f);
                    
                    // Tangent in U direction
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float dBasisU = bernsteinBasisDerivative(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float basisV = bernsteinBasis(controlPointsV - 1, j, vParam);
                            tangentU += controlPoints[i][j] * dBasisU * basisV;
                        }
                    }
                    
                    // Tangent in V direction
                    for (int i = 0; i < controlPointsU; ++i)
                    {
                        float basisU = bernsteinBasis(controlPointsU - 1, i, uParam);
                        for (int j = 0; j < controlPointsV; ++j)
                        {
                            float dBasisV = bernsteinBasisDerivative(controlPointsV - 1, j, vParam);
                            tangentV += controlPoints[i][j] * basisU * dBasisV;
                        }
                    }
                    
                    glm::vec3 normal = glm::normalize(glm::cross(tangentU, tangentV));
                    if (glm::length(normal) < 0.001f)
                    {
                        normal = glm::vec3(0.0f, 0.0f, 1.0f);
                    }
                    
                    Vertex vertex;
                    vertex.position = position;
                    vertex.normal = normal;
                    vertex.uv = glm::vec2(uParam, 1.0f - vParam);
                    vertex.color = glm::vec3(1.0f);
                    
                    vertices.push_back(vertex);
                }
            }
            
            return vertices;
        }
        
        namespace
        {
            void buildControlPointMarkerGeometry(
                const std::vector<glm::vec3>& controlPoints, float markerSize, const glm::vec3& color,
                std::vector<Vertex>& outVertices, std::vector<uint32_t>* outIndices)
            {
                outVertices.clear();
                if (outIndices != nullptr)
                {
                    outIndices->clear();
                }
                
                const float halfSize = markerSize * 0.5f;
                const std::array<glm::vec3, 6> offsets = {
                    glm::vec3(0.0f,  halfSize, 0.0f),
                    glm::vec3( halfSize, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f,  halfSize),
                    glm::vec3(-halfSize, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, -halfSize),
                    glm::vec3(0.0f, -halfSize, 0.0f)
                };
                
                const std::array<glm::ivec3, 8> faces = {
                    glm::ivec3(0, 1, 2),
                    glm::ivec3(0, 2, 3),
                    glm::ivec3(0, 3, 4),
                    glm::ivec3(0, 4, 1),
                    glm::ivec3(5, 2, 1),
                    glm::ivec3(5, 3, 2),
                    glm::ivec3(5, 4, 3),
                    glm::ivec3(5, 1, 4)
                };
                
                outVertices.reserve(controlPoints.size() * faces.size() * 3);
                if (outIndices != nullptr)
                {
                    outIndices->reserve(controlPoints.size() * faces.size() * 3);
                }
                
                uint32_t baseIndex = 0;
                for (const auto& center : controlPoints)
                {
                    for (const auto& face : faces)
                    {
                        glm::vec3 p0 = center + offsets[face.x];
                        glm::vec3 p1 = center + offsets[face.y];
                        glm::vec3 p2 = center + offsets[face.z];
                        
                        glm::vec3 normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z) ||
                            glm::length(normal) < 1e-4f)
                        {
                            normal = glm::vec3(0.0f, 1.0f, 0.0f);
                        }
                        
                        Vertex v0;
                        v0.position = p0;
                        v0.normal = normal;
                        v0.uv = glm::vec2(0.0f);
                        v0.color = color;
                        
                        Vertex v1 = v0;
                        v1.position = p1;
                        
                        Vertex v2 = v0;
                        v2.position = p2;
                        
                        outVertices.push_back(v0);
                        outVertices.push_back(v1);
                        outVertices.push_back(v2);
                        
                        if (outIndices != nullptr)
                        {
                            outIndices->push_back(baseIndex);
                            outIndices->push_back(baseIndex + 1);
                            outIndices->push_back(baseIndex + 2);
                        }
                        
                        baseIndex += 3;
                    }
                }
            }
        }
        
        Mesh generateFlagControlPointDebugMesh(
            float width, float height, int controlPointsU, int controlPointsV,
            float markerSize, const glm::vec3& color)
        {
            Mesh mesh;
            mesh.name = "flag_control_points";
            
            controlPointsU = std::max(2, controlPointsU);
            controlPointsV = std::max(2, controlPointsV);
            
            std::vector<glm::vec3> controlPoints;
            controlPoints.reserve(controlPointsU * controlPointsV);
            
            float halfWidth = width * 0.5f;
            float halfHeight = height * 0.5f;
            
            for (int i = 0; i < controlPointsU; ++i)
            {
                float u = static_cast<float>(i) / static_cast<float>(controlPointsU - 1);
                float x = glm::mix(-halfWidth, halfWidth, u);
                
                for (int j = 0; j < controlPointsV; ++j)
                {
                    float v = static_cast<float>(j) / static_cast<float>(controlPointsV - 1);
                    float y = glm::mix(-halfHeight, halfHeight, v);
                    controlPoints.push_back(glm::vec3(x, y, 0.0f));
                }
            }
            
            buildControlPointMarkerGeometry(controlPoints, markerSize, color, mesh.vertices, &mesh.indices);
            mesh.transform = glm::mat4(1.0f);
            return mesh;
        }
        
        void updateFlagControlPointDebugVertices(
            const std::vector<glm::vec3>& controlPoints, float markerSize, const glm::vec3& color,
            std::vector<Vertex>& outVertices)
        {
            buildControlPointMarkerGeometry(controlPoints, markerSize, color, outVertices, nullptr);
        }
        
        std::vector<Mesh> generateFlagpole(float height, float poleRadius, float ballRadius, 
                                           int segments, const glm::vec3& poleColor, const glm::vec3& ballColor)
        {
            std::vector<Mesh> meshes;
            
            // Generate cylinder (pole)
            Mesh pole;
            pole.name = "flagpole_pole";
            pole.vertices.reserve((segments + 1) * 2);
            pole.indices.reserve(segments * 6);
            pole.diffuseTexture = "models/FlagPole/jinshu.jpg";
            
            // Generate vertices for cylinder
            for (int i = 0; i <= segments; ++i)
            {
                float angle = static_cast<float>(i) / static_cast<float>(segments) * glm::two_pi<float>();
                float x = std::cos(angle) * poleRadius;
                float z = std::sin(angle) * poleRadius;
                
                glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
                
                // Bottom vertex
                Vertex bottomVertex;
                bottomVertex.position = glm::vec3(x, 0.0f, z);
                bottomVertex.normal = normal;
                bottomVertex.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 0.0f);
                bottomVertex.color = poleColor;
                pole.vertices.push_back(bottomVertex);
                
                // Top vertex
                Vertex topVertex;
                topVertex.position = glm::vec3(x, height, z);
                topVertex.normal = normal;
                topVertex.uv = glm::vec2(static_cast<float>(i) / static_cast<float>(segments), 1.0f);
                topVertex.color = poleColor;
                pole.vertices.push_back(topVertex);
            }
            
            // Generate indices for cylinder sides
            for (int i = 0; i < segments; ++i)
            {
                int bottom0 = i * 2;
                int bottom1 = (i + 1) * 2;
                int top0 = i * 2 + 1;
                int top1 = (i + 1) * 2 + 1;
                
                // First triangle
                pole.indices.push_back(bottom0);
                pole.indices.push_back(top0);
                pole.indices.push_back(bottom1);
                
                // Second triangle
                pole.indices.push_back(bottom1);
                pole.indices.push_back(top0);
                pole.indices.push_back(top1);
            }
            
            // Add bottom and top caps
            // Bottom cap center
            Vertex bottomCenter;
            bottomCenter.position = glm::vec3(0.0f, 0.0f, 0.0f);
            bottomCenter.normal = glm::vec3(0.0f, -1.0f, 0.0f);
            bottomCenter.uv = glm::vec2(0.5f, 0.5f);
            bottomCenter.color = poleColor;
            uint32_t bottomCenterIdx = static_cast<uint32_t>(pole.vertices.size());
            pole.vertices.push_back(bottomCenter);
            
            // Top cap center
            Vertex topCenter;
            topCenter.position = glm::vec3(0.0f, height, 0.0f);
            topCenter.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            topCenter.uv = glm::vec2(0.5f, 0.5f);
            topCenter.color = poleColor;
            uint32_t topCenterIdx = static_cast<uint32_t>(pole.vertices.size());
            pole.vertices.push_back(topCenter);
            
            // Bottom cap triangles
            for (int i = 0; i < segments; ++i)
            {
                int idx0 = i * 2;
                int idx1 = (i + 1) * 2;
                pole.indices.push_back(bottomCenterIdx);
                pole.indices.push_back(idx1);
                pole.indices.push_back(idx0);
            }
            
            // Top cap triangles
            for (int i = 0; i < segments; ++i)
            {
                int idx0 = i * 2 + 1;
                int idx1 = (i + 1) * 2 + 1;
                pole.indices.push_back(topCenterIdx);
                pole.indices.push_back(idx0);
                pole.indices.push_back(idx1);
            }
            
            pole.transform = glm::mat4(1.0f);
            meshes.push_back(std::move(pole));
            
            // Generate sphere (ball on top)
            Mesh ball;
            ball.name = "flagpole_ball";
            ball.diffuseTexture = "models/FlagPole/jinshu.jpg";
            
            // Generate sphere vertices
            const int sphereSegments = segments;
            const int sphereRings = segments / 2;
            
            for (int ring = 0; ring <= sphereRings; ++ring)
            {
                float theta = static_cast<float>(ring) / static_cast<float>(sphereRings) * glm::pi<float>();
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);
                
                for (int seg = 0; seg <= sphereSegments; ++seg)
                {
                    float phi = static_cast<float>(seg) / static_cast<float>(sphereSegments) * glm::two_pi<float>();
                    float sinPhi = std::sin(phi);
                    float cosPhi = std::cos(phi);
                    
                    Vertex vertex;
                    vertex.position = glm::vec3(
                        cosPhi * sinTheta * ballRadius,
                        cosTheta * ballRadius,
                        sinPhi * sinTheta * ballRadius
                    ) + glm::vec3(0.0f, height, 0.0f);  // Position at top of pole
                    
                    vertex.normal = glm::normalize(glm::vec3(
                        cosPhi * sinTheta,
                        cosTheta,
                        sinPhi * sinTheta
                    ));
                    
                    vertex.uv = glm::vec2(
                        static_cast<float>(seg) / static_cast<float>(sphereSegments),
                        static_cast<float>(ring) / static_cast<float>(sphereRings)
                    );
                    vertex.color = ballColor;
                    
                    ball.vertices.push_back(vertex);
                }
            }
            
            // Generate sphere indices
            for (int ring = 0; ring < sphereRings; ++ring)
            {
                for (int seg = 0; seg < sphereSegments; ++seg)
                {
                    int current = ring * (sphereSegments + 1) + seg;
                    int next = current + sphereSegments + 1;
                    
                    // First triangle
                    ball.indices.push_back(current);
                    ball.indices.push_back(next);
                    ball.indices.push_back(current + 1);
                    
                    // Second triangle
                    ball.indices.push_back(current + 1);
                    ball.indices.push_back(next);
                    ball.indices.push_back(next + 1);
                }
            }
            
            ball.transform = glm::mat4(1.0f);
            meshes.push_back(std::move(ball));
            
            return meshes;
        }
    }
} // namespace cg

