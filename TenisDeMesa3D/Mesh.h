
#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <assimp/types.h>

#include "Shader.h"

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture
{
    GLuint id = 0;
    std::string type;
    aiString path;
};

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<Texture> textures;
    glm::vec3 baseColor;

    Mesh(
        const std::vector<Vertex>& vertices,
        const std::vector<GLuint>& indices,
        const std::vector<Texture>& textures,
        const glm::vec3& baseColor = glm::vec3(1.0f)
    )
        : vertices(vertices),
        indices(indices),
        textures(textures),
        baseColor(baseColor)
    {
        setupMesh();
    }

    void Draw(Shader shader)
    {
        bool hasDiffuseTexture = false;
        GLuint diffuseNumber = 1;
        GLuint specularNumber = 1;

        for (GLuint i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            const std::string& name = textures[i].type;

            if (name == "texture_diffuse")
            {
                number = std::to_string(diffuseNumber++);

                if (textures[i].id != 0)
                    hasDiffuseTexture = true;
            }
            else if (name == "texture_specular")
            {
                number = std::to_string(specularNumber++);
            }

            const std::string uniformName = name + number;

            glUniform1i(
                glGetUniformLocation(
                    shader.Program,
                    uniformName.c_str()
                ),
                static_cast<GLint>(i)
            );

            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        // El material de cada malla conserva su color propio.
        glUniform3fv(
            glGetUniformLocation(shader.Program, "baseColor"),
            1,
            &baseColor[0]
        );

        glUniform1i(
            glGetUniformLocation(
                shader.Program, "hasDiffuseTexture"
            ),
            hasDiffuseTexture ? 1 : 0
        );

        // La textura difusa utiliza la unidad 0.
        glUniform1i(
            glGetUniformLocation(
                shader.Program, "texture_diffuse1"
            ),
            0
        );

        glBindVertexArray(VAO);

        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT,
            nullptr
        );

        glBindVertexArray(0);

        // Desenlazar las texturas utilizadas por esta malla.
        for (GLuint i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glActiveTexture(GL_TEXTURE0);
    }

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                vertices.size() * sizeof(Vertex)
                ),
            vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                indices.size() * sizeof(GLuint)
                ),
            indices.data(),
            GL_STATIC_DRAW
        );

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, Position)
        );

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, Normal)
        );

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, TexCoords)
        );

        glBindVertexArray(0);
    }
};
