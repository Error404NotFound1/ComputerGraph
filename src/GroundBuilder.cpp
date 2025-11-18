#include "scene/GroundBuilder.h"
#include "loader/ObjLoader.h"
#include "util/MeshUtils.h"
#include "util/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <random>
#include <map>
#include <limits>

namespace cg
{
    namespace GroundBuilder
    {
        namespace
        {
            struct GroundTilePrototype
            {
                Mesh mesh;
                MeshUtils::MeshBounds bounds;
                float weight{1.0f};
                bool decoration{false};
            };

            struct TileClassification
            {
                float weight{1.0f};
                bool decoration{false};
            };

            TileClassification classifyTile(const std::string& name)
            {
                std::string lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                
                // STRICT FILTER: Only keep pure brick tiles (Fragment, Slab)
                // Explicitly exclude: grass, ground, and any decorations
                // This ensures only brick floor tiles are used, no decorations (stones, grass, etc.)
                
                // First check: explicitly exclude decorations (grass, ground, etc.)
                // These contain stones and weeds that we don't want
                if (lower.find("grass") != std::string::npos || 
                    lower.find("ground") != std::string::npos ||
                    lower.find("deco") != std::string::npos || 
                    lower.find("detail") != std::string::npos ||
                    lower.find("ornament") != std::string::npos)
                {
                    // Return weight 0.0f to exclude these tiles completely
                    return {0.0f, true};
                }
                
                // Second check: only include Fragment or Slab tiles (pure brick tiles)
                // Must contain "fragment" or "slab" in the name to be included
                // These are the only clean brick floor tiles without decorations
                if (lower.find("fragment") != std::string::npos || 
                    lower.find("slab") != std::string::npos)
                {
                    return {1.0f, false};
                }
                
                // If it doesn't match Fragment or Slab, exclude it (safety check)
                // This ensures we only use known brick tile types
                return {0.0f, true};
            }

            std::string selectTexturePath(const std::filesystem::path& texturesDir, const std::string& meshName)
            {
                auto ensure = [&](const std::string& file) -> std::string
                {
                    const auto full = texturesDir / file;
                    if (std::filesystem::exists(full))
                    {
                        return full.generic_string();
                    }
                    return {};
                };

                std::string lower = meshName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower.find("grass") != std::string::npos)
                {
                    if (auto tex = ensure("Grass_Diffuse.jpg"); !tex.empty()) return tex;
                    if (auto tex = ensure("Grass_Diffuse.tga"); !tex.empty()) return tex;
                }

                if (lower.find("detail") != std::string::npos)
                {
                    if (auto tex = ensure("StoneFloorDetails_Diffuse.jpg"); !tex.empty()) return tex;
                }

                if (auto tex = ensure("StoneFloor_Diffuse.jpg"); !tex.empty()) return tex;
                if (auto fallback = ensure("StoneFloorGrass_Diffuse.tga"); !fallback.empty()) return fallback;
                return {};
            }

            std::vector<GroundTilePrototype> loadGroundTileSet(const std::string& groundPath)
            {
                namespace fs = std::filesystem;
                std::vector<GroundTilePrototype> tiles;
                if (groundPath.empty())
                {
                    return tiles;
                }

                const fs::path path(groundPath);
                if (!fs::exists(path))
                {
                    log(LogLevel::Warn, "Ground path not found: " + groundPath);
                    return tiles;
                }

                fs::path texturesDir = path;
                if (!fs::is_directory(texturesDir))
                {
                    texturesDir = texturesDir.parent_path();
                }
                texturesDir = texturesDir / "Textures";

                // First pass: load all candidate tiles (Fragment and Slab)
                std::vector<GroundTilePrototype> candidateTiles;
                if (fs::is_directory(path))
                {
                    std::vector<fs::path> objFiles;
                    for (const auto& entry : fs::directory_iterator(path))
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".obj")
                        {
                            objFiles.push_back(entry.path());
                        }
                    }
                    std::sort(objFiles.begin(), objFiles.end());
                    for (const auto& objPath : objFiles)
                    {
                        // Pre-filter by filename before loading to save time and ensure strict filtering
                        std::string fileName = objPath.filename().string();
                        std::string lowerFileName = fileName;
                        std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
                        
                        // Skip LOD versions, only load base models (Fragment.obj, Slab.obj, not _LOD1.obj, _LOD2.obj)
                        // Skip if it's a decoration (grass, ground, etc.)
                        if (lowerFileName.find("_lod") != std::string::npos ||
                            lowerFileName.find("grass") != std::string::npos || 
                            lowerFileName.find("ground") != std::string::npos ||
                            (lowerFileName.find("fragment") == std::string::npos && lowerFileName.find("slab") == std::string::npos))
                        {
                            log(LogLevel::Info, "Skipping file (not a base Fragment/Slab tile): " + fileName);
                            continue;
                        }
                        
                        if (auto mesh = ObjLoader::loadObjAsMesh(objPath.string()))
                        {
                            GroundTilePrototype proto;
                            proto.bounds = MeshUtils::computeBounds(*mesh);
                            auto classification = classifyTile(mesh->name);
                            proto.weight = classification.weight;
                            proto.decoration = classification.decoration;
                            if (fs::exists(texturesDir))
                            {
                                mesh->diffuseTexture = selectTexturePath(texturesDir, mesh->name);
                            }
                            proto.mesh = std::move(*mesh);
                            // Collect all candidates first (even if weight > 0)
                            if (proto.weight > 0.0f)
                            {
                                candidateTiles.push_back(std::move(proto));
                            }
                        }
                    }
                }
                else
                {
                    if (auto mesh = ObjLoader::loadObjAsMesh(path.string()))
                    {
                        GroundTilePrototype proto;
                        proto.bounds = MeshUtils::computeBounds(*mesh);
                        auto classification = classifyTile(mesh->name);
                        proto.weight = classification.weight;
                        proto.decoration = classification.decoration;
                        if (fs::exists(texturesDir))
                        {
                            mesh->diffuseTexture = selectTexturePath(texturesDir, mesh->name);
                        }
                        proto.mesh = std::move(*mesh);
                        if (proto.weight > 0.0f)
                        {
                            candidateTiles.push_back(std::move(proto));
                        }
                    }
                }

                // Second pass: filter by size to remove small decorations (stones, etc.)
                // Calculate average size of all candidate tiles
                if (!candidateTiles.empty())
                {
                    float totalArea = 0.0f;
                    std::vector<float> areas;
                    areas.reserve(candidateTiles.size());
                    
                    for (const auto& proto : candidateTiles)
                    {
                        const glm::vec3 ext = proto.bounds.extent();
                        // Calculate horizontal area (X * Z, ignoring Y height)
                        float area = ext.x * ext.z;
                        areas.push_back(area);
                        totalArea += area;
                    }
                    
                    float avgArea = totalArea / static_cast<float>(candidateTiles.size());
                    // Filter threshold: tiles smaller than 30% of average are considered decorations (stones)
                    float sizeThreshold = avgArea * 0.3f;
                    
                    log(LogLevel::Info, "Ground tile size analysis: avg area=" + std::to_string(avgArea) + 
                        ", threshold=" + std::to_string(sizeThreshold));
                    
                    for (size_t i = 0; i < candidateTiles.size(); ++i)
                    {
                        const auto& proto = candidateTiles[i];
                        float area = areas[i];
                        
                        if (area >= sizeThreshold)
                        {
                            // This is a proper brick tile, add it
                            tiles.push_back(proto);
                            log(LogLevel::Info, "Added ground tile: " + proto.mesh.name + 
                                " (area=" + std::to_string(area) + ", weight=" + std::to_string(proto.weight) + ")");
                        }
                        else
                        {
                            // This is too small, likely a decoration stone, skip it
                            log(LogLevel::Info, "Skipped small decoration tile: " + proto.mesh.name + 
                                " (area=" + std::to_string(area) + " < threshold " + std::to_string(sizeThreshold) + ")");
                        }
                    }
                }

                log(LogLevel::Info, "Loaded " + std::to_string(tiles.size()) + " ground tile prototypes (after size filtering)");
                return tiles;
            }

            glm::vec2 computeCellExtent(const std::vector<GroundTilePrototype>& prototypes)
            {
                glm::vec2 extent(0.0f);
                for (const auto& proto : prototypes)
                {
                    const glm::vec3 ext = proto.bounds.extent();
                    extent.x = std::max(extent.x, ext.x);
                    extent.y = std::max(extent.y, ext.z);
                }
                return extent;
            }

            std::vector<Mesh> createGroundGrid(const std::vector<GroundTilePrototype>& prototypes, int gridX, int gridZ, const glm::vec2& cellExtent)
            {
                std::vector<Mesh> result;
                if (prototypes.empty() || gridX <= 0 || gridZ <= 0)
                {
                    return result;
                }

                float totalWeight = 0.0f;
                for (const auto& proto : prototypes)
                {
                    totalWeight += proto.weight;
                }

                std::mt19937 rng(12345);
                std::uniform_real_distribution<float> weightDist(0.0f, totalWeight);
                std::uniform_real_distribution<float> rotationDist(0.0f, 1.0f);

                const float offsetX = cellExtent.x * static_cast<float>(gridX) * 0.5f;
                const float offsetZ = cellExtent.y * static_cast<float>(gridZ) * 0.5f;

                for (int gx = 0; gx < gridX; ++gx)
                {
                    for (int gz = 0; gz < gridZ; ++gz)
                    {
                        float randomWeight = weightDist(rng);
                        float accumulated = 0.0f;
                        const GroundTilePrototype* selected = nullptr;
                        for (const auto& proto : prototypes)
                        {
                            accumulated += proto.weight;
                            if (randomWeight <= accumulated)
                            {
                                selected = &proto;
                                break;
                            }
                        }

                        if (!selected) continue;

                        const float tileCenterX = static_cast<float>(gx) * cellExtent.x - offsetX + cellExtent.x * 0.5f;
                        const float tileCenterZ = static_cast<float>(gz) * cellExtent.y - offsetZ + cellExtent.y * 0.5f;

                        int rotation = static_cast<int>(rotationDist(rng) * 4);
                        glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(tileCenterX, 0.0f, tileCenterZ));
                        transform = glm::rotate(transform, rotation * glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
                        transform = glm::translate(transform, -selected->bounds.center());

                        Mesh instance = selected->mesh;
                        instance.transform = transform;
                        result.push_back(std::move(instance));
                    }
                }

                return result;
            }

            std::vector<Mesh> createDecorationMeshes(const std::vector<GroundTilePrototype>& decorations, int gridX, int gridZ, const glm::vec2& cellExtent)
            {
                std::vector<Mesh> result;
                if (decorations.empty() || gridX <= 0 || gridZ <= 0)
                {
                    return result;
                }

                const float probability = 0.01f;
                std::mt19937 rng(5678);
                std::uniform_real_distribution<float> chance(0.0f, 1.0f);
                std::uniform_int_distribution<size_t> decoDist(0, decorations.size() - 1);
                std::uniform_int_distribution<int> rotationDist(0, 3);
                std::uniform_real_distribution<float> jitter(-0.2f, 0.2f);

                const float offsetX = cellExtent.x * static_cast<float>(gridX) * 0.5f;
                const float offsetZ = cellExtent.y * static_cast<float>(gridZ) * 0.5f;

                for (int gx = 0; gx < gridX; ++gx)
                {
                    for (int gz = 0; gz < gridZ; ++gz)
                    {
                        if (chance(rng) > probability) continue;

                        const auto& proto = decorations[decoDist(rng)];
                        const float tileCenterX = static_cast<float>(gx) * cellExtent.x - offsetX + cellExtent.x * 0.5f;
                        const float tileCenterZ = static_cast<float>(gz) * cellExtent.y - offsetZ + cellExtent.y * 0.5f;

                        bool horizontal = chance(rng) < 0.5f;
                        float posX = tileCenterX;
                        float posZ = tileCenterZ;
                        if (horizontal)
                        {
                            posZ += (chance(rng) < 0.5f ? -0.5f : 0.5f) * cellExtent.y * 0.9f;
                            posX += jitter(rng) * cellExtent.x;
                        }
                        else
                        {
                            posX += (chance(rng) < 0.5f ? -0.5f : 0.5f) * cellExtent.x * 0.9f;
                            posZ += jitter(rng) * cellExtent.y;
                        }

                        glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(posX, 0.0f, posZ));
                        transform = glm::rotate(transform, rotationDist(rng) * glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
                        transform = glm::translate(transform, -proto.bounds.center());

                        Mesh instance = proto.mesh;
                        instance.transform = transform;
                        result.push_back(std::move(instance));
                    }
                }

                if (!result.empty())
                {
                    log(LogLevel::Info, "Created " + std::to_string(result.size()) + " decoration meshes");
                }

                return result;
            }
        }

        std::vector<Mesh> buildDemoScene(const std::string& groundMeshPath, int tilesPerSide)
        {
            std::vector<Mesh> meshes;
            meshes.reserve(64);

            if (!groundMeshPath.empty())
            {
                auto prototypes = loadGroundTileSet(groundMeshPath);
                if (!prototypes.empty())
                {
                    std::vector<GroundTilePrototype> mainTiles;
                    std::vector<GroundTilePrototype> decoTiles;
                    mainTiles.reserve(prototypes.size());
                    decoTiles.reserve(prototypes.size());

                    for (auto& proto : prototypes)
                    {
                        if (proto.decoration)
                        {
                            decoTiles.push_back(std::move(proto));
                        }
                        else
                        {
                            mainTiles.push_back(std::move(proto));
                        }
                    }

                    if (mainTiles.empty())
                    {
                        log(LogLevel::Warn, "No primary ground tiles found in " + groundMeshPath);
                    }
                    else
                    {
                        glm::vec2 cellExtent = computeCellExtent(mainTiles);
                        const int gridCount = std::max(1, tilesPerSide);

                        auto tiles = createGroundGrid(mainTiles, gridCount, gridCount, cellExtent);
                        meshes.reserve(meshes.size() + tiles.size());
                        meshes.insert(meshes.end(), std::make_move_iterator(tiles.begin()), std::make_move_iterator(tiles.end()));

                        if (!decoTiles.empty())
                        {
                            auto decoMeshes = createDecorationMeshes(decoTiles, gridCount, gridCount, cellExtent);
                            meshes.insert(meshes.end(), std::make_move_iterator(decoMeshes.begin()), std::make_move_iterator(decoMeshes.end()));
                        }
                    }
                }
                else
                {
                    log(LogLevel::Warn, "No ground tiles could be loaded from " + groundMeshPath);
                }
            }

            return meshes;
        }
    }
} // namespace cg

