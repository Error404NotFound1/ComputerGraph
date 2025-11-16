#include "scene/DemoSceneBuilder.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <random>

#include "util/Log.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace cg
{
    namespace
    {
        Vertex makeVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv, const glm::vec3& color)
        {
            Vertex v{};
            v.position = position;
            v.normal = normal;
            v.uv = uv;
            v.color = color;
            return v;
        }

        struct MeshBounds
        {
            glm::vec3 min{std::numeric_limits<float>::max()};
            glm::vec3 max{std::numeric_limits<float>::lowest()};
            glm::vec3 extent() const { return max - min; }
            glm::vec3 center() const { return (min + max) * 0.5f; }
        };

        struct GroundTilePrototype
        {
            Mesh mesh;
            MeshBounds bounds;
            float weight{1.0f};
            bool decoration{false};
        };

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

        struct TileClassification
        {
            float weight;
            bool decoration;
        };

        TileClassification classifyTile(const std::string& name)
        {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const bool isSlab = lower.find("slab") != std::string::npos;
            const bool isGrass = lower.find("grass") != std::string::npos;
            const bool isFragment = lower.find("fragment") != std::string::npos;
            const bool isGround = lower.find("ground") != std::string::npos;

            if (isSlab)
            {
                return {1.0f, false};
            }

            if (isGrass || isFragment || isGround)
            {
                return {0.05f, true};
            }

            return {0.1f, true};
        }

        std::string selectTexturePath(const std::filesystem::path& texturesDir, const std::string& meshName)
        {
            auto ensure = [&](const std::string& file) -> std::string
            {
                const auto full = texturesDir / file;
                if (std::filesystem::exists(full))
                {
                    return full.string();
                }
                return {};
            };

            std::string lower = meshName;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lower.find("grass") != std::string::npos)
            {
                if (auto tex = ensure("StoneFloorGrass_Diffuse.tga"); !tex.empty()) return tex;
            }
            if (lower.find("fragment") != std::string::npos || lower.find("ground") != std::string::npos)
            {
                if (auto tex = ensure("StoneFloorDetails_Diffuse.jpg"); !tex.empty()) return tex;
            }

            if (auto tex = ensure("StoneFloor_Diffuse.jpg"); !tex.empty()) return tex;
            if (auto fallback = ensure("StoneFloorGrass_Diffuse.tga"); !fallback.empty()) return fallback;
            return {};
        }

        Mesh createMountainRing(int segments,
                                float innerRadius,
                                float outerRadius,
                                float minHeight,
                                float maxHeight,
                                const glm::vec3& baseColor,
                                const glm::vec3& peakColor)
        {
            Mesh mesh;
            mesh.name = "Mountains";
            mesh.vertices.reserve((segments + 1) * 2);

            for (int i = 0; i <= segments; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(segments);
                const float angle = t * glm::two_pi<float>();
                const float heightFactor = 0.5f + 0.5f * sinf(angle * 3.5f);
                const float ridgeHeight = glm::mix(minHeight, maxHeight, heightFactor);
                const float normalTilt = glm::mix(0.3f, 0.8f, heightFactor);
                const glm::vec3 dir{cosf(angle), 0.0f, sinf(angle)};
                const glm::vec3 normal = glm::normalize(glm::vec3(dir.x, normalTilt, dir.z));

                const glm::vec3 innerPos = dir * innerRadius;
                const glm::vec3 outerPos = dir * outerRadius + glm::vec3(0.0f, ridgeHeight, 0.0f);

                mesh.vertices.push_back(makeVertex(innerPos, normal, glm::vec2(t, 0.0f), baseColor));
                mesh.vertices.push_back(makeVertex(outerPos, normal, glm::vec2(t, 1.0f), glm::mix(baseColor, peakColor, heightFactor)));
            }

            for (int i = 0; i < segments; ++i)
            {
                const uint32_t baseIndex = static_cast<uint32_t>(i * 2);
                const uint32_t nextIndex = static_cast<uint32_t>(((i + 1) % (segments + 1)) * 2);

                mesh.indices.insert(mesh.indices.end(),
                                    {baseIndex,
                                     nextIndex,
                                     nextIndex + 1,
                                     baseIndex,
                                     nextIndex + 1,
                                     baseIndex + 1});
            }

            return mesh;
        }

        void addQuad(Mesh& mesh,
                     const glm::vec3& a,
                     const glm::vec3& b,
                     const glm::vec3& c,
                     const glm::vec3& d,
                     const glm::vec3& normal,
                     const glm::vec3& color)
        {
            const uint32_t start = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(makeVertex(a, normal, glm::vec2(0.0f, 0.0f), color));
            mesh.vertices.push_back(makeVertex(b, normal, glm::vec2(1.0f, 0.0f), color));
            mesh.vertices.push_back(makeVertex(c, normal, glm::vec2(1.0f, 1.0f), color));
            mesh.vertices.push_back(makeVertex(d, normal, glm::vec2(0.0f, 1.0f), color));

            mesh.indices.insert(mesh.indices.end(),
                                {start, start + 1, start + 2,
                                 start, start + 2, start + 3});
        }

        Mesh createBuilding(const glm::vec3& position,
                            const glm::vec2& footprint,
                            float height,
                            const glm::vec3& sideColor,
                            const glm::vec3& roofColor)
        {
            Mesh mesh;
            mesh.name = "Building";

            const glm::vec3 half{footprint.x * 0.5f, height, footprint.y * 0.5f};
            const glm::vec3 p000 = position + glm::vec3(-half.x, 0.0f, -half.z);
            const glm::vec3 p010 = position + glm::vec3(-half.x, height, -half.z);
            const glm::vec3 p100 = position + glm::vec3(half.x, 0.0f, -half.z);
            const glm::vec3 p110 = position + glm::vec3(half.x, height, -half.z);
            const glm::vec3 p001 = position + glm::vec3(-half.x, 0.0f, half.z);
            const glm::vec3 p011 = position + glm::vec3(-half.x, height, half.z);
            const glm::vec3 p101 = position + glm::vec3(half.x, 0.0f, half.z);
            const glm::vec3 p111 = position + glm::vec3(half.x, height, half.z);

            addQuad(mesh, p000, p100, p110, p010, glm::vec3(0.0f, 0.0f, -1.0f), sideColor);
            addQuad(mesh, p101, p001, p011, p111, glm::vec3(0.0f, 0.0f, 1.0f), sideColor);
            addQuad(mesh, p001, p000, p010, p011, glm::vec3(-1.0f, 0.0f, 0.0f), sideColor);
            addQuad(mesh, p100, p101, p111, p110, glm::vec3(1.0f, 0.0f, 0.0f), sideColor);
            addQuad(mesh, p010, p110, p111, p011, glm::vec3(0.0f, 1.0f, 0.0f), roofColor);

            return mesh;
        }

        std::vector<Mesh> createCity()
        {
            std::vector<Mesh> meshes;
            meshes.reserve(25);

            std::mt19937 rng(1337);
            std::uniform_real_distribution<float> heightDist(80.0f, 260.0f);
            std::uniform_real_distribution<float> colorJitter(-0.05f, 0.05f);
            std::uniform_real_distribution<float> keepDist(0.0f, 1.0f);

            const float spacing = 220.0f;

            for (int x = -2; x <= 2; ++x)
            {
                for (int z = -2; z <= 2; ++z)
                {
                    if (std::abs(x) + std::abs(z) > 3) continue;
                    // Randomly skip some buildings to reduce the number of cubes in the world
                    if (keepDist(rng) < 0.5f) continue;

                    const glm::vec3 basePosition{static_cast<float>(x) * spacing, 0.0f, static_cast<float>(z) * spacing};
                    const glm::vec2 footprint{150.0f - std::abs(static_cast<float>(z)) * 20.0f,
                                              150.0f - std::abs(static_cast<float>(x)) * 20.0f};
                    const float height = heightDist(rng);
                    const glm::vec3 sideColor = glm::clamp(glm::vec3(0.55f, 0.58f, 0.6f) + colorJitter(rng), 0.2f, 0.9f);
                    const glm::vec3 roofColor = glm::clamp(sideColor + glm::vec3(0.08f, 0.08f, 0.05f), 0.0f, 1.0f);

                    meshes.push_back(createBuilding(basePosition, footprint, height, sideColor, roofColor));
                }
            }

            return meshes;
        }

        std::optional<Mesh> loadObjMesh(const std::string& path)
        {
            if (path.empty())
            {
                return std::nullopt;
            }

            const std::filesystem::path filePath(path);
            if (!std::filesystem::exists(filePath))
            {
                log(LogLevel::Error, "Ground mesh not found: " + path);
                return std::nullopt;
            }

            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn;
            std::string err;
            const std::string baseDir = filePath.parent_path().string();

            const bool loaded = tinyobj::LoadObj(
                &attrib,
                &shapes,
                &materials,
                &warn,
                &err,
                path.c_str(),
                baseDir.empty() ? nullptr : baseDir.c_str(),
                true);

            if (!warn.empty())
            {
                log(LogLevel::Warn, "TinyObjLoader warn: " + warn);
            }

            if (!loaded)
            {
                log(LogLevel::Error, "TinyObjLoader failed: " + err);
                return std::nullopt;
            }

            Mesh mesh;
            mesh.name = filePath.filename().string();
            mesh.vertices.reserve(shapes.size() * 3);
            mesh.indices.reserve(shapes.size() * 3);

            size_t totalFaces = 0;
            for (const auto& shape : shapes)
            {
                size_t indexOffset = 0;
                for (const auto& faceVertexCount : shape.mesh.num_face_vertices)
                {
                    for (size_t v = 0; v < faceVertexCount; ++v)
                    {
                        const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                        Vertex vertex{};
                        if (idx.vertex_index >= 0)
                        {
                            vertex.position = glm::vec3(
                                attrib.vertices[3 * idx.vertex_index + 0],
                                attrib.vertices[3 * idx.vertex_index + 1],
                                attrib.vertices[3 * idx.vertex_index + 2]);
                        }
                        if (idx.normal_index >= 0)
                        {
                            vertex.normal = glm::vec3(
                                attrib.normals[3 * idx.normal_index + 0],
                                attrib.normals[3 * idx.normal_index + 1],
                                attrib.normals[3 * idx.normal_index + 2]);
                        }
                        else
                        {
                            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                        }
                        if (idx.texcoord_index >= 0)
                        {
                            vertex.uv = glm::vec2(
                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                attrib.texcoords[2 * idx.texcoord_index + 1]);
                        }
                        vertex.color = glm::vec3(1.0f);
                        mesh.vertices.push_back(vertex);
                        mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
                    }
                    indexOffset += faceVertexCount;
                    ++totalFaces;
                }
            }

            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                log(LogLevel::Error, "Ground mesh is empty: " + path);
                return std::nullopt;
            }

            const MeshBounds rawBounds = computeBounds(mesh);
            const glm::vec3 rawExtent = rawBounds.extent();
            const bool looksZUp = rawExtent.z < rawExtent.y * 0.25f;
            if (looksZUp)
            {
                for (auto& vertex : mesh.vertices)
                {
                    vertex.position = glm::vec3(vertex.position.x, vertex.position.z, vertex.position.y);
                    vertex.normal = glm::normalize(glm::vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y));
                }
                log(LogLevel::Info, "Rotated mesh '" + mesh.name + "' from Z-up to Y-up orientation");
            }

            mesh.transform = glm::mat4(1.0f);
            log(LogLevel::Info, "Loaded ground mesh '" + mesh.name + "' (" + std::to_string(mesh.vertices.size()) + " verts, " + std::to_string(totalFaces) + " faces)");
            return mesh;
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
                    if (auto mesh = loadObjMesh(objPath.string()))
                    {
                        GroundTilePrototype proto;
                        proto.bounds = computeBounds(*mesh);
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
                            tiles.push_back(std::move(proto));
                        }
                    }
                }
            }
            else
            {
                if (auto mesh = loadObjMesh(path.string()))
                {
                    GroundTilePrototype proto;
                    proto.bounds = computeBounds(*mesh);
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
                        tiles.push_back(std::move(proto));
                    }
                }
            }

            log(LogLevel::Info, "Loaded " + std::to_string(tiles.size()) + " ground tile prototypes");
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

            if (cellExtent.x <= 0.0f || cellExtent.y <= 0.0f || totalWeight <= 0.0f)
            {
                log(LogLevel::Warn, "Ground tile extent invalid");
                return result;
            }

            result.reserve(static_cast<size_t>(gridX) * gridZ);
            std::mt19937 rng(1234);
            std::uniform_real_distribution<float> weightDist(0.0f, totalWeight);
            std::uniform_int_distribution<int> rotationDist(0, 3);

            const float offsetX = cellExtent.x * static_cast<float>(gridX) * 0.5f;
            const float offsetZ = cellExtent.y * static_cast<float>(gridZ) * 0.5f;

            for (int gx = 0; gx < gridX; ++gx)
            {
                for (int gz = 0; gz < gridZ; ++gz)
                {
                    float roll = weightDist(rng);
                    const GroundTilePrototype* chosen = nullptr;
                    for (const auto& proto : prototypes)
                    {
                        if (roll <= proto.weight)
                        {
                            chosen = &proto;
                            break;
                        }
                        roll -= proto.weight;
                    }
                    if (!chosen)
                    {
                        chosen = &prototypes.back();
                    }

                    const float posX = static_cast<float>(gx) * cellExtent.x - offsetX + cellExtent.x * 0.5f;
                    const float posZ = static_cast<float>(gz) * cellExtent.y - offsetZ + cellExtent.y * 0.5f;

                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(posX, 0.0f, posZ));
                    const int rotationSteps = rotationDist(rng);
                    transform = glm::rotate(transform, rotationSteps * glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
                    transform = glm::translate(transform, -chosen->bounds.center());
                    Mesh tile = chosen->mesh;
                    tile.transform = transform;
                    result.push_back(std::move(tile));
                }
            }

            log(LogLevel::Info, "Generated ground grid with " + std::to_string(result.size()) + " tiles");
            return result;
        }

        std::vector<Mesh> createDecorationMeshes(const std::vector<GroundTilePrototype>& decorations, int gridX, int gridZ, const glm::vec2& cellExtent)
        {
            std::vector<Mesh> result;
            if (decorations.empty() || gridX <= 0 || gridZ <= 0 || cellExtent.x <= 0.0f || cellExtent.y <= 0.0f)
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
                log(LogLevel::Info, "Placed " + std::to_string(result.size()) + " decoration tiles");
            }
            return result;
        }
    } // namespace

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

        // Skip procedural city cubes entirely

        return meshes;
    }

	std::optional<Mesh> loadObjAsMesh(const std::string& path)
	{
		// Duplicate internal loader to expose for general use (rocket, etc.)
		if (path.empty())
		{
			return std::nullopt;
		}

		const std::filesystem::path filePath(path);
		if (!std::filesystem::exists(filePath))
		{
			log(LogLevel::Error, "OBJ not found: " + path);
			return std::nullopt;
		}

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn;
		std::string err;
		const std::string baseDir = filePath.parent_path().string();

		const bool loaded = tinyobj::LoadObj(
			&attrib,
			&shapes,
			&materials,
			&warn,
			&err,
			path.c_str(),
			baseDir.empty() ? nullptr : baseDir.c_str(),
			true);

		if (!warn.empty())
		{
			log(LogLevel::Warn, "TinyObjLoader warn: " + warn);
		}

		if (!loaded)
		{
			log(LogLevel::Error, "TinyObjLoader failed: " + err);
			return std::nullopt;
		}

		Mesh mesh;
		mesh.name = filePath.filename().string();
		mesh.vertices.reserve(shapes.size() * 3);
		mesh.indices.reserve(shapes.size() * 3);

		for (const auto& shape : shapes)
		{
			size_t indexOffset = 0;
			for (const auto& faceVertexCount : shape.mesh.num_face_vertices)
			{
				for (size_t v = 0; v < faceVertexCount; ++v)
				{
					const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
					Vertex vertex{};
					if (idx.vertex_index >= 0)
					{
						vertex.position = glm::vec3(
							attrib.vertices[3 * idx.vertex_index + 0],
							attrib.vertices[3 * idx.vertex_index + 1],
							attrib.vertices[3 * idx.vertex_index + 2]);
					}
					if (idx.normal_index >= 0)
					{
						vertex.normal = glm::vec3(
							attrib.normals[3 * idx.normal_index + 0],
							attrib.normals[3 * idx.normal_index + 1],
							attrib.normals[3 * idx.normal_index + 2]);
					}
					else
					{
						vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
					}
					if (idx.texcoord_index >= 0)
					{
						vertex.uv = glm::vec2(
							attrib.texcoords[2 * idx.texcoord_index + 0],
							attrib.texcoords[2 * idx.texcoord_index + 1]);
					}
					vertex.color = glm::vec3(1.0f);
					mesh.vertices.push_back(vertex);
					mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
				}
				indexOffset += faceVertexCount;
			}
		}

		if (mesh.vertices.empty() || mesh.indices.empty())
		{
			log(LogLevel::Error, "OBJ mesh is empty: " + path);
			return std::nullopt;
		}

		const MeshBounds rawBounds = computeBounds(mesh);
		const glm::vec3 rawExtent = rawBounds.extent();
		const bool looksZUp = rawExtent.z < rawExtent.y * 0.25f;
		if (looksZUp)
		{
			for (auto& vertex : mesh.vertices)
			{
				vertex.position = glm::vec3(vertex.position.x, vertex.position.z, vertex.position.y);
				vertex.normal = glm::normalize(glm::vec3(vertex.normal.x, vertex.normal.z, vertex.normal.y));
			}
			log(LogLevel::Info, "Rotated mesh '" + mesh.name + "' from Z-up to Y-up orientation");
		}

		mesh.transform = glm::mat4(1.0f);
		log(LogLevel::Info, "Loaded OBJ '" + mesh.name + "' (" + std::to_string(mesh.vertices.size()) + " verts)");
		return mesh;
	}
} // namespace cg


