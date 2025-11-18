#include "loader/ObjLoader.h"
#include "util/MeshUtils.h"
#include "util/Log.h"

#include <glm/glm.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <chrono>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace cg
{
    namespace ObjLoader
    {
        namespace
        {
        }

        std::optional<Mesh> loadObjAsMesh(const std::string& path)
        {
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

            // Optimized: read entire file into memory first, then parse from memory
            std::ifstream fileStream(path, std::ios::binary | std::ios::ate);
            if (!fileStream)
            {
                log(LogLevel::Error, "Cannot open OBJ file: " + path);
                return std::nullopt;
            }
            
            std::streamsize fileSize = fileStream.tellg();
            fileStream.seekg(0, std::ios::beg);
            
            std::vector<char> fileBuffer(static_cast<size_t>(fileSize));
            if (!fileStream.read(fileBuffer.data(), fileSize))
            {
                log(LogLevel::Error, "Failed to read OBJ file: " + path);
                return std::nullopt;
            }
            fileStream.close();
            
            // Parse from memory stream
            std::istringstream objStream(std::string(fileBuffer.data(), static_cast<size_t>(fileSize)));
            tinyobj::MaterialFileReader matFileReader(baseDir);
            
            const bool loaded = tinyobj::LoadObj(
                &attrib,
                &shapes,
                &materials,
                &warn,
                &err,
                &objStream,
                &matFileReader,
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

            bool hasNormals = false;
            
            // First pass: check if normals exist
            for (const auto& shape : shapes)
            {
                size_t indexOffset = 0;
                for (size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx)
                {
                    const auto& faceVertexCount = shape.mesh.num_face_vertices[faceIdx];
                    for (size_t v = 0; v < faceVertexCount; ++v)
                    {
                        const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                        if (idx.normal_index >= 0)
                        {
                            hasNormals = true;
                            break;
                        }
                    }
                    if (hasNormals) break;
                    indexOffset += faceVertexCount;
                }
                if (hasNormals) break;
            }
            
            // Second pass: load vertices
            for (const auto& shape : shapes)
            {
                size_t indexOffset = 0;
                for (size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx)
                {
                    const auto& faceVertexCount = shape.mesh.num_face_vertices[faceIdx];
                    
                    // Get material for this face
                    int materialId = -1;
                    if (faceIdx < shape.mesh.material_ids.size())
                    {
                        materialId = shape.mesh.material_ids[faceIdx];
                    }
                    
                    // Get diffuse color from material (default to dark gray if no material)
                    glm::vec3 faceColor = glm::vec3(0.2f, 0.2f, 0.2f);
                    if (materialId >= 0 && materialId < static_cast<int>(materials.size()))
                    {
                        const auto& mat = materials[materialId];
                        if (mat.diffuse[0] > 0.0f || mat.diffuse[1] > 0.0f || mat.diffuse[2] > 0.0f)
                        {
                            faceColor = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                        }
                        else if (mat.ambient[0] > 0.0f || mat.ambient[1] > 0.0f || mat.ambient[2] > 0.0f)
                        {
                            faceColor = glm::vec3(mat.ambient[0], mat.ambient[1], mat.ambient[2]);
                        }
                    }
                    
                    bool hasVertexColors = !attrib.colors.empty();
                    
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
                            
                            if (hasVertexColors && idx.vertex_index >= 0 && 
                                3 * idx.vertex_index + 2 < attrib.colors.size())
                            {
                                vertex.color = glm::vec3(
                                    attrib.colors[3 * idx.vertex_index + 0],
                                    attrib.colors[3 * idx.vertex_index + 1],
                                    attrib.colors[3 * idx.vertex_index + 2]);
                            }
                            else
                            {
                                vertex.color = faceColor;
                            }
                        }
                        else
                        {
                            vertex.color = faceColor;
                        }
                        
                        if (idx.normal_index >= 0)
                        {
                            vertex.normal = glm::vec3(
                                attrib.normals[3 * idx.normal_index + 0],
                                attrib.normals[3 * idx.normal_index + 1],
                                attrib.normals[3 * idx.normal_index + 2]);
                        }
                        
                        if (idx.texcoord_index >= 0)
                        {
                            vertex.uv = glm::vec2(
                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                attrib.texcoords[2 * idx.texcoord_index + 1]);
                        }
                        
                        mesh.vertices.push_back(vertex);
                        mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
                    }
                    indexOffset += faceVertexCount;
                }
            }
            
            // Third pass: calculate normals if missing using MeshUtils
            if (!hasNormals && mesh.vertices.size() >= 3)
            {
                MeshUtils::calculateNormals(mesh);
            }

            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                log(LogLevel::Error, "OBJ mesh is empty: " + path);
                return std::nullopt;
            }

            const MeshUtils::MeshBounds rawBounds = MeshUtils::computeBounds(mesh);
            const glm::vec3 rawExtent = rawBounds.extent();
            const bool looksZUp = rawExtent.z < rawExtent.y * 0.25f;
            if (looksZUp)
            {
                MeshUtils::transformZUpToYUp(mesh);
                log(LogLevel::Info, "Rotated mesh '" + mesh.name + "' from Z-up to Y-up orientation");
            }

            mesh.transform = glm::mat4(1.0f);
            log(LogLevel::Info, "Loaded OBJ '" + mesh.name + "' (" + std::to_string(mesh.vertices.size()) + " verts)");
            return mesh;
        }

        std::vector<Mesh> loadObjAsMeshes(const std::string& path)
        {
            std::vector<Mesh> meshes;
            
            if (path.empty())
            {
                return meshes;
            }

            const std::filesystem::path filePath(path);
            if (!std::filesystem::exists(filePath))
            {
                log(LogLevel::Error, "OBJ not found: " + path);
                return meshes;
            }

            // Record loading start time
            std::chrono::high_resolution_clock::time_point loadStart = std::chrono::high_resolution_clock::now();
            log(LogLevel::Info, "Starting to load model: " + path);

            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn;
            std::string err;
            const std::string baseDir = filePath.parent_path().string();

            // Optimized file reading: read entire file into memory first, then parse from memory
            std::chrono::high_resolution_clock::time_point fileReadStart = std::chrono::high_resolution_clock::now();
            
            // Read entire OBJ file into memory in one operation
            std::ifstream fileStream(path, std::ios::binary | std::ios::ate);
            if (!fileStream)
            {
                log(LogLevel::Error, "Cannot open OBJ file: " + path);
                return meshes;
            }
            
            std::streamsize fileSize = fileStream.tellg();
            fileStream.seekg(0, std::ios::beg);
            
            std::vector<char> fileBuffer(static_cast<size_t>(fileSize));
            if (!fileStream.read(fileBuffer.data(), fileSize))
            {
                log(LogLevel::Error, "Failed to read OBJ file: " + path);
                return meshes;
            }
            fileStream.close();
            
            std::chrono::high_resolution_clock::time_point fileReadEnd = std::chrono::high_resolution_clock::now();
            long long fileReadTime = std::chrono::duration_cast<std::chrono::milliseconds>(fileReadEnd - fileReadStart).count();
            log(LogLevel::Info, "File read into memory, time: " + std::to_string(fileReadTime) + "ms, size: " + 
                std::to_string(fileSize / 1024 / 1024) + "MB");
            
            // Parse from memory stream (much faster than reading from disk line by line)
            std::istringstream objStream(std::string(fileBuffer.data(), static_cast<size_t>(fileSize)));
            tinyobj::MaterialFileReader matFileReader(baseDir);
            
            std::chrono::high_resolution_clock::time_point parseStart = std::chrono::high_resolution_clock::now();
            const bool loaded = tinyobj::LoadObj(
                &attrib,
                &shapes,
                &materials,
                &warn,
                &err,
                &objStream,
                &matFileReader,
                true);
            std::chrono::high_resolution_clock::time_point parseEnd = std::chrono::high_resolution_clock::now();
            long long parseTime = std::chrono::duration_cast<std::chrono::milliseconds>(parseEnd - parseStart).count();
            log(LogLevel::Info, "File parsing completed, time: " + std::to_string(parseTime) + "ms");

            if (!warn.empty())
            {
                log(LogLevel::Warn, "TinyObjLoader warn: " + warn);
            }

            if (!loaded)
            {
                log(LogLevel::Error, "TinyObjLoader failed: " + err);
                return meshes;
            }

            // Check if normals exist
            bool hasNormals = false;
            for (const auto& shape : shapes)
            {
                size_t indexOffset = 0;
                for (size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx)
                {
                    const auto& faceVertexCount = shape.mesh.num_face_vertices[faceIdx];
                    for (size_t v = 0; v < faceVertexCount; ++v)
                    {
                        const tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                        if (idx.normal_index >= 0)
                        {
                            hasNormals = true;
                            break;
                        }
                    }
                    if (hasNormals) break;
                    indexOffset += faceVertexCount;
                }
                if (hasNormals) break;
            }

            // Map material ID to mesh index
            std::map<int, size_t> materialToMeshIndex;
            
            // First pass: collect all faces grouped by material
            for (const auto& shape : shapes)
            {
                size_t indexOffset = 0;
                for (size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); ++faceIdx)
                {
                    const auto& faceVertexCount = shape.mesh.num_face_vertices[faceIdx];
                    
                    // Get material for this face
                    int materialId = -1;
                    if (faceIdx < shape.mesh.material_ids.size())
                    {
                        materialId = shape.mesh.material_ids[faceIdx];
                    }
                    
                    // Find or create mesh for this material
                    size_t meshIndex = 0;
                    if (materialToMeshIndex.find(materialId) == materialToMeshIndex.end())
                    {
                        // Create new mesh for this material
                        Mesh newMesh;
                        newMesh.name = filePath.filename().string() + "_mat_" + std::to_string(materialId);
                        if (materialId >= 0 && materialId < static_cast<int>(materials.size()))
                        {
                            const auto& mat = materials[materialId];
                            // Set texture path from material
                            if (!mat.diffuse_texname.empty())
                            {
                                std::filesystem::path texPath = std::filesystem::path(baseDir) / mat.diffuse_texname;
                                if (std::filesystem::exists(texPath))
                                {
                                    newMesh.diffuseTexture = texPath.generic_string();
                                    log(LogLevel::Info, "Material " + std::to_string(materialId) + " uses texture: " + newMesh.diffuseTexture);
                                }
                                else
                                {
                                    // Try to find texture file with different case or extension
                                    std::string texName = mat.diffuse_texname;
                                    std::transform(texName.begin(), texName.end(), texName.begin(), ::tolower);
                                    
                                    // Search in base directory first
                                    bool found = false;
                                    for (const auto& entry : std::filesystem::directory_iterator(baseDir))
                                    {
                                        if (entry.is_regular_file())
                                        {
                                            std::string entryName = entry.path().filename().string();
                                            std::string entryNameLower = entryName;
                                            std::transform(entryNameLower.begin(), entryNameLower.end(), entryNameLower.begin(), ::tolower);
                                            
                                            if (entryNameLower == texName || entryNameLower.find(texName) != std::string::npos)
                                            {
                                                newMesh.diffuseTexture = entry.path().generic_string();
                                                log(LogLevel::Info, "Found texture for material " + std::to_string(materialId) + ": " + newMesh.diffuseTexture);
                                                found = true;
                                                break;
                                            }
                                        }
                                    }
                                    
                                    // If not found, try searching in subdirectories
                                    if (!found)
                                    {
                                        for (const auto& entry : std::filesystem::directory_iterator(baseDir))
                                        {
                                            if (entry.is_directory())
                                            {
                                                for (const auto& subEntry : std::filesystem::directory_iterator(entry.path()))
                                                {
                                                    if (subEntry.is_regular_file())
                                                    {
                                                        std::string entryName = subEntry.path().filename().string();
                                                        std::string entryNameLower = entryName;
                                                        std::transform(entryNameLower.begin(), entryNameLower.end(), entryNameLower.begin(), ::tolower);
                                                        
                                                        if (entryNameLower == texName || entryNameLower.find(texName) != std::string::npos)
                                                        {
                                                            newMesh.diffuseTexture = subEntry.path().generic_string();
                                                            log(LogLevel::Info, "Found texture for material " + std::to_string(materialId) + " in subdirectory: " + newMesh.diffuseTexture);
                                                            found = true;
                                                            break;
                                                        }
                                                    }
                                                }
                                                if (found) break;
                                            }
                                        }
                                    }
                                    
                                    if (!found)
                                    {
                                        log(LogLevel::Warn, "Texture not found for material " + std::to_string(materialId) + ": " + mat.diffuse_texname + " (mesh will use vertex colors)");
                                    }
                                }
                            }
                        }
                        meshes.push_back(newMesh);
                        meshIndex = meshes.size() - 1;
                        materialToMeshIndex[materialId] = meshIndex;
                    }
                    else
                    {
                        meshIndex = materialToMeshIndex[materialId];
                    }
                    
                    Mesh& mesh = meshes[meshIndex];
                    
                    // Get diffuse color from material
                    glm::vec3 faceColor = glm::vec3(0.2f, 0.2f, 0.2f);
                    if (materialId >= 0 && materialId < static_cast<int>(materials.size()))
                    {
                        const auto& mat = materials[materialId];
                        if (mat.diffuse[0] > 0.0f || mat.diffuse[1] > 0.0f || mat.diffuse[2] > 0.0f)
                        {
                            faceColor = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
                        }
                        else if (mat.ambient[0] > 0.0f || mat.ambient[1] > 0.0f || mat.ambient[2] > 0.0f)
                        {
                            faceColor = glm::vec3(mat.ambient[0], mat.ambient[1], mat.ambient[2]);
                        }
                    }
                    
                    bool hasVertexColors = !attrib.colors.empty();
                    
                    // Add vertices for this face
                    size_t faceStartIndex = mesh.vertices.size();
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
                            
                            if (hasVertexColors && idx.vertex_index >= 0 && 
                                3 * idx.vertex_index + 2 < attrib.colors.size())
                            {
                                vertex.color = glm::vec3(
                                    attrib.colors[3 * idx.vertex_index + 0],
                                    attrib.colors[3 * idx.vertex_index + 1],
                                    attrib.colors[3 * idx.vertex_index + 2]);
                            }
                            else
                            {
                                vertex.color = faceColor;
                            }
                        }
                        else
                        {
                            vertex.color = faceColor;
                        }
                        
                        if (idx.normal_index >= 0)
                        {
                            vertex.normal = glm::vec3(
                                attrib.normals[3 * idx.normal_index + 0],
                                attrib.normals[3 * idx.normal_index + 1],
                                attrib.normals[3 * idx.normal_index + 2]);
                        }
                        
                        if (idx.texcoord_index >= 0)
                        {
                            vertex.uv = glm::vec2(
                                attrib.texcoords[2 * idx.texcoord_index + 0],
                                attrib.texcoords[2 * idx.texcoord_index + 1]);
                        }
                        
                        mesh.vertices.push_back(vertex);
                        mesh.indices.push_back(static_cast<uint32_t>(mesh.vertices.size() - 1));
                    }
                    
                    indexOffset += faceVertexCount;
                }
            }
            
            // Log mesh information before processing
            log(LogLevel::Info, "Created " + std::to_string(meshes.size()) + " meshes from materials");
            for (size_t i = 0; i < meshes.size(); ++i)
            {
                log(LogLevel::Info, "Mesh " + std::to_string(i) + ": name='" + meshes[i].name + 
                    "', vertices=" + std::to_string(meshes[i].vertices.size()) + 
                    ", indices=" + std::to_string(meshes[i].indices.size()) +
                    ", texture='" + meshes[i].diffuseTexture + "'");
            }
            
            // Calculate normals for meshes that don't have them
            for (auto& mesh : meshes)
            {
                if (!hasNormals && mesh.vertices.size() >= 3)
                {
                    MeshUtils::calculateNormals(mesh);
                }
                
                // Apply Z-up to Y-up transformation if needed using MeshUtils
                const MeshUtils::MeshBounds rawBounds = MeshUtils::computeBounds(mesh);
                const glm::vec3 rawExtent = rawBounds.extent();
                const bool looksZUp = rawExtent.z < rawExtent.y * 0.25f;
                if (looksZUp)
                {
                    MeshUtils::transformZUpToYUp(mesh);
                }
                
                mesh.transform = glm::mat4(1.0f);
            }
            
            // Remove empty meshes (meshes with no vertices or indices)
            meshes.erase(std::remove_if(meshes.begin(), meshes.end(), 
                [](const Mesh& m) { return m.vertices.empty() || m.indices.empty(); }), 
                meshes.end());
            
            log(LogLevel::Info, "After removing empty meshes: " + std::to_string(meshes.size()) + " meshes remain");
            
            std::chrono::high_resolution_clock::time_point loadEnd = std::chrono::high_resolution_clock::now();
            long long totalLoadTime = std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart).count();
            
            size_t totalVertices = 0;
            size_t totalFaces = 0;
            for (const auto& mesh : meshes)
            {
                totalVertices += mesh.vertices.size();
                totalFaces += mesh.indices.size() / 3;
            }
            
            log(LogLevel::Info, "Model loading completed: " + filePath.filename().string() + 
                " | Total time: " + std::to_string(totalLoadTime) + "ms" +
                " | Mesh count: " + std::to_string(meshes.size()) +
                " | Vertices: " + std::to_string(totalVertices) +
                " | Faces: " + std::to_string(totalFaces));
            log(LogLevel::Info, "Model loading optimized: files are read into memory first, then parsed from memory for better I/O performance");
            
            return meshes;
        }
    }
} // namespace cg

