
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <limits>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "SOIL2/SOIL2.h"
#include "Mesh.h"
#include "Shader.h"

class Model
{
public:
    explicit Model(GLchar* path, bool grayUntexturedMeshes = false)
        : grayUntexturedMeshes(grayUntexturedMeshes)
    {
        loadModel(path);
    }

    glm::vec3 GetCenter() const
    {
        if (!hasBounds)
            return glm::vec3(0.0f);

        return (boundsMin + boundsMax) * 0.5f;
    }

    float GetMinY() const
    {
        return hasBounds ? boundsMin.y : 0.0f;
    }

    float GetMaxExtent() const
    {
        if (!hasBounds)
            return 0.0f;

        const glm::vec3 size = boundsMax - boundsMin;
        return glm::max(size.x, glm::max(size.y, size.z));
    }

    void Draw(Shader shader)
    {
        for (Mesh& mesh : meshes)
            mesh.Draw(shader);
    }

private:
    std::vector<Mesh> meshes;
    std::string directory;
    std::vector<Texture> texturesLoaded;
    glm::vec3 boundsMin = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    bool hasBounds = false;
    bool grayUntexturedMeshes = false;

    void loadModel(const std::string& path)
    {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals
        );

        if (!scene ||
            (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
            !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP:: "
                << importer.GetErrorString() << '\n';
            return;
        }

        const size_t separator = path.find_last_of("/\\");

        directory = (separator == std::string::npos)
            ? "."
            : path.substr(0, separator);

        std::cout << "Cargando modelo: " << path << '\n';
        std::cout << "Directorio: " << directory << '\n';

        processNode(scene->mRootNode, scene);

        std::cout << "Modelo cargado: " << path
            << " | Mallas: " << meshes.size() << '\n';
    }

    void processNode(aiNode* node, const aiScene* scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i], scene);
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
        std::vector<Texture> textures;

        vertices.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex{};

            vertex.Position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );

            boundsMin = glm::min(boundsMin, vertex.Position);
            boundsMax = glm::max(boundsMax, vertex.Position);
            hasBounds = true;

            if (mesh->HasNormals())
            {
                vertex.Normal = glm::vec3(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                );
            }
            else
            {
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoords = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f);
            }

            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            const aiFace& face = mesh->mFaces[i];

            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        glm::vec3 baseColor(0.8f, 0.8f, 0.8f);

        if (mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiMaterial* material =
                scene->mMaterials[mesh->mMaterialIndex];

            aiColor3D diffuseColor(0.8f, 0.8f, 0.8f);

            if (material->Get(
                AI_MATKEY_COLOR_DIFFUSE,
                diffuseColor
            ) == AI_SUCCESS)
            {
                baseColor = glm::vec3(
                    diffuseColor.r,
                    diffuseColor.g,
                    diffuseColor.b
                );
            }

            std::vector<Texture> diffuseMaps =
                loadMaterialTextures(
                    material,
                    aiTextureType_DIFFUSE,
                    "texture_diffuse"
                );

            textures.insert(
                textures.end(),
                diffuseMaps.begin(),
                diffuseMaps.end()
            );

            // The table's non-textured pieces use neutral gray, while the
            // arena and props keep the colors authored in their MTL files.
            if (grayUntexturedMeshes && diffuseMaps.empty())
                baseColor = glm::vec3(0.65f);

            std::vector<Texture> specularMaps =
                loadMaterialTextures(
                    material,
                    aiTextureType_SPECULAR,
                    "texture_specular"
                );

            textures.insert(
                textures.end(),
                specularMaps.begin(),
                specularMaps.end()
            );
        }

        return Mesh(vertices, indices, textures, baseColor);
    }

    std::vector<Texture> loadMaterialTextures(
        aiMaterial* material,
        aiTextureType type,
        const std::string& typeName
    )
    {
        std::vector<Texture> textures;

        for (unsigned int i = 0;
            i < material->GetTextureCount(type); i++)
        {
            aiString str;

            if (material->GetTexture(type, i, &str) != AI_SUCCESS)
                continue;

            bool alreadyLoaded = false;

            for (const Texture& loaded : texturesLoaded)
            {
                if (loaded.path == str)
                {
                    textures.push_back(loaded);
                    alreadyLoaded = true;
                    break;
                }
            }

            if (alreadyLoaded)
                continue;

            Texture texture{};
            texture.id = TextureFromFile(str.C_Str(), directory);
            texture.type = typeName;
            texture.path = str;

            if (texture.id != 0)
            {
                textures.push_back(texture);
                texturesLoaded.push_back(texture);
            }
        }

        return textures;
    }

    GLuint TextureFromFile(
        const char* path,
        const std::string& directory
    )
    {
        std::string filename(path);

        const bool absolutePath =
            (!filename.empty() &&
                (filename[0] == '/' || filename[0] == '\\')) ||
            (filename.size() > 1 && filename[1] == ':');

        if (!absolutePath)
            filename = directory + "/" + filename;

        int width = 0;
        int height = 0;
        int channels = 0;

        unsigned char* image = SOIL_load_image(
            filename.c_str(),
            &width,
            &height,
            &channels,
            SOIL_LOAD_RGBA
        );

        if (!image)
        {
            std::cerr << "ERROR::TEXTURE:: No se pudo cargar: "
                << filename << '\n';
            return 0;
        }

        GLuint textureID = 0;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA,
            width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, image
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT
        );
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT
        );
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            GL_LINEAR_MIPMAP_LINEAR
        );
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR
        );

        glBindTexture(GL_TEXTURE_2D, 0);
        SOIL_free_image_data(image);

        std::cout << "Textura cargada: " << filename << '\n';

        return textureID;
    }
};
