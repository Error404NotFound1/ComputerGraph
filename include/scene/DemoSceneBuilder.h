#pragma once

#include "scene/Scene.h"

#include <vector>
#include <optional>

namespace cg
{
    // Procedurally builds a demo scene and optionally layers in a tiled ground mesh.
    std::vector<Mesh> buildDemoScene(const std::string& groundMeshPath, int tilesPerSide);
	// Load an arbitrary OBJ file into a Mesh (Y-up normalization applied like ground tiles).
	// Returns std::nullopt on failure.
	std::optional<Mesh> loadObjAsMesh(const std::string& path);
} // namespace cg


