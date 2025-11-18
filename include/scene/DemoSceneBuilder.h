#pragma once

#include "scene/Scene.h"
#include "scene/FlagGenerator.h"
#include "scene/GroundBuilder.h"
#include "loader/ObjLoader.h"

#include <vector>
#include <optional>
#include <glm/glm.hpp>

namespace cg
{
    // Legacy compatibility wrapper - delegates to modular implementations
    // Procedurally builds a demo scene and optionally layers in a tiled ground mesh.
    inline std::vector<Mesh> buildDemoScene(const std::string& groundMeshPath, int tilesPerSide)
    {
        return GroundBuilder::buildDemoScene(groundMeshPath, tilesPerSide);
    }
    
    // Load an arbitrary OBJ file into a Mesh (Y-up normalization applied like ground tiles).
    // Returns std::nullopt on failure.
    inline std::optional<Mesh> loadObjAsMesh(const std::string& path)
    {
        return ObjLoader::loadObjAsMesh(path);
    }
    
    // Load an arbitrary OBJ file and split by materials into multiple Meshes.
    // Each Mesh will have its corresponding texture from the MTL file.
    // Returns empty vector on failure.
    inline std::vector<Mesh> loadObjAsMeshes(const std::string& path)
    {
        return ObjLoader::loadObjAsMeshes(path);
    }
    
    // Generate a Bezier surface flag mesh for waving animation (with configurable control points)
    inline Mesh generateBezierFlag(float width, float height, int controlPointsU, int controlPointsV,
                                    int segmentsU, int segmentsV)
    {
        return FlagGenerator::generateBezierFlag(width, height, controlPointsU, controlPointsV, segmentsU, segmentsV);
    }
    
    // Legacy wrapper (uses default 4x4 control points)
    inline Mesh generateBezierFlag(float width, float height, int segmentsU, int segmentsV)
    {
        return FlagGenerator::generateBezierFlag(width, height, 4, 4, segmentsU, segmentsV);
    }
    
    // Update flag vertices with waving animation (with configurable control points)
    inline std::vector<Vertex> updateFlagVertices(
        float width, float height, int controlPointsU, int controlPointsV,
        int segmentsU, int segmentsV, float animationTime, float waveAmplitude, float waveFrequency,
        std::vector<glm::vec3>* outControlPoints = nullptr)
    {
        return FlagGenerator::updateFlagVertices(width, height, controlPointsU, controlPointsV,
            segmentsU, segmentsV, animationTime, waveAmplitude, waveFrequency, outControlPoints);
    }
    
    // Legacy wrapper (uses default 4x4 control points, includes windStrength for backward compatibility)
    inline std::vector<Vertex> updateFlagVertices(
        float width, float height, int segmentsU, int segmentsV,
        float animationTime, float waveAmplitude, float waveFrequency, float windStrength,
        std::vector<glm::vec3>* outControlPoints = nullptr)
    {
        // Note: windStrength parameter is ignored in new implementation (use waveAmplitude instead)
        return FlagGenerator::updateFlagVertices(width, height, 4, 4, segmentsU, segmentsV,
            animationTime, waveAmplitude, waveFrequency, outControlPoints);
    }
} // namespace cg


