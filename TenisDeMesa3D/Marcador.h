#pragma once
// ============================================================================
//  Marcador.h  -  Puntaje dinámico del marcador (dígitos tipo display LED)
//
//  Reemplaza los "0" fijos del modelo (Puntaje_Rojo / Puntaje_Azul de
//  marcador.obj, que ya se quitaron del .obj). Cada dígito se arma con 7
//  segmentos; acepta 0..99 (1 o 2 cifras) y tiene un destello al anotar.
//  Usa el color emisivo del material "Digitos" para que brille.
// ============================================================================
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "Shader.h"

class MarcadorDigital
{
public:
    // Debe llamarse con el contexto OpenGL ya creado (después de glewInit)
    void Init()
    {
        segments.clear();
        for (int s = 0; s < 7; ++s) segments.push_back(makeSegment(s));
    }

    // board: matriz del marcador en el mundo (la misma con que se dibuja su modelo)
    void Draw(Shader& shader, GLint modelLoc, const glm::mat4& board,
              int rojo, int azul, float flashRojo, float flashAzul)
    {
        // Posición de los antiguos "0" en el espacio local del modelo
        const float cy = 1.5635f;
        drawNumber(shader, modelLoc, board, rojo,  0.300f, cy,
                   glm::vec3(1.000f, 0.035f, 0.025f), glm::vec3(2.00f, 0.07f, 0.05f), flashRojo);
        drawNumber(shader, modelLoc, board, azul, -0.300f, cy,
                   glm::vec3(0.040f, 0.350f, 1.000f), glm::vec3(0.08f, 0.70f, 2.00f), flashAzul);
    }

private:
    std::vector<Mesh> segments;   // 0=A arriba, 1=B arriba-der, 2=C abajo-der, 3=D abajo, 4=E abajo-izq, 5=F arriba-izq, 6=G medio

    // Segmentos encendidos por dígito (bit0 = A ... bit6 = G)
    static int mask(int d)
    {
        static const int M[10] = {
            0x3F, // 0: ABCDEF
            0x06, // 1: BC
            0x5B, // 2: ABGED
            0x4F, // 3: ABGCD
            0x66, // 4: FGBC
            0x6D, // 5: AFGCD
            0x7D, // 6: AFGEDC
            0x07, // 7: ABC
            0x7F, // 8: todos
            0x6F  // 9: ABCDFG
        };
        return M[d];
    }

    // Segmento hexagonal en una celda de 1 x 2 (z = 0, normal +Z, CCW visto desde +Z)
    static Mesh makeSegment(int s)
    {
        const float h = 0.10f;   // medio espesor
        std::vector<glm::vec2> p;
        auto horiz = [&](float yc) {
            const float x0 = 0.13f, x1 = 0.87f;
            p = { {x0, yc}, {x0 + h, yc - h}, {x1 - h, yc - h}, {x1, yc}, {x1 - h, yc + h}, {x0 + h, yc + h} };
        };
        auto vert = [&](float xc, float y0, float y1) {
            p = { {xc, y0}, {xc + h, y0 + h}, {xc + h, y1 - h}, {xc, y1}, {xc - h, y1 - h}, {xc - h, y0 + h} };
        };
        switch (s)
        {
        case 0: horiz(1.90f); break;
        case 1: vert(0.90f, 1.03f, 1.87f); break;
        case 2: vert(0.90f, 0.13f, 0.97f); break;
        case 3: horiz(0.10f); break;
        case 4: vert(0.10f, 0.13f, 0.97f); break;
        case 5: vert(0.10f, 1.03f, 1.87f); break;
        default: horiz(1.00f); break;
        }

        std::vector<Vertex> vertices;
        for (const glm::vec2& q : p)
        {
            Vertex v;
            v.Position = glm::vec3(q.x, q.y, 0.0f);
            v.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
            v.TexCoords = glm::vec2(0.0f);
            vertices.push_back(v);
        }
        std::vector<GLuint> indices = { 0, 1, 2,  0, 2, 3,  0, 3, 4,  0, 4, 5 };
        std::vector<Texture> noTextures;
        return Mesh(vertices, indices, noTextures, glm::vec3(1.0f), glm::vec3(0.0f));
    }

    void drawNumber(Shader& shader, GLint modelLoc, const glm::mat4& board, int value,
                    float cx, float cy, const glm::vec3& base, const glm::vec3& emis, float flash)
    {
        value = value < 0 ? 0 : (value > 99 ? 99 : value);

        int digits[2];
        int count;
        if (value >= 10) { count = 2; digits[0] = value / 10; digits[1] = value % 10; }
        else             { count = 1; digits[0] = value; }

        float s = (count == 1) ? 0.0735f : 0.057f;     // la celda mide s de ancho y 2*s de alto
        s *= 1.0f + 0.18f * flash;                      // late al anotar
        const float gap = 0.20f * s;
        const float totalW = count * s + (count - 1) * gap;
        const float x0 = cx - 0.5f * totalW;

        for (int d = 0; d < count; ++d)
        {
            float dx = x0 + d * (s + gap);
            if (digits[d] == 1) dx -= 0.40f * s;        // el "1" queda centrado en su celda

            glm::mat4 model = board;
            model = glm::translate(model, glm::vec3(dx, cy - s, 0.1168f));   // un poco delante de la pantalla
            model = glm::scale(model, glm::vec3(s, s, 1.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);

            const int m = mask(digits[d]);
            for (int seg = 0; seg < 7; ++seg)
            {
                if (!(m & (1 << seg))) continue;
                segments[seg].baseColor = base;
                segments[seg].emissiveColor = emis * (1.0f + 2.0f * flash);
                segments[seg].Draw(shader);
            }
        }
    }
};
