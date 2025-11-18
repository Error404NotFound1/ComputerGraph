#pragma once

#include "scene/Scene.h"

#include <string>
#include <vector>

namespace cg
{
    namespace GroundBuilder
    {
        // Build a demo scene with tiled ground meshes
        std::vector<Mesh> buildDemoScene(const std::string& groundMeshPath, int tilesPerSide);
    }
} // namespace cg

