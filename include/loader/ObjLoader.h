#pragma once

#include "scene/Scene.h"

#include <string>
#include <vector>
#include <optional>

namespace cg
{
    namespace ObjLoader
    {
        // Load an OBJ file into a single Mesh
        // Returns std::nullopt on failure
        std::optional<Mesh> loadObjAsMesh(const std::string& path);
        
        // Load an OBJ file and split by materials into multiple Meshes
        // Each Mesh will have its corresponding texture from the MTL file
        // Returns empty vector on failure
        std::vector<Mesh> loadObjAsMeshes(const std::string& path);
    }
} // namespace cg

