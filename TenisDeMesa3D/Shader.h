
#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include <GL/glew.h>

class Shader
{
public:
    GLuint Program = 0;
    GLint uniformColor = -1;

    Shader(
        const GLchar* vertexPath,
        const GLchar* fragmentPath
    )
    {
        std::string vertexCode;
        std::string fragmentCode;

        std::cout
            << "\n====================================\n"
            << "CARGANDO SHADERS\n"
            << "====================================\n";

        // ----------------------------------------------------
        // 1. MOSTRAR RUTAS Y DIRECTORIO DE EJECUCION
        // ----------------------------------------------------

        std::cout
            << "Directorio de ejecucion:\n"
            << std::filesystem::current_path().string()
            << "\n\n";

        std::cout
            << "VERTEX SHADER:\n"
            << std::filesystem::absolute(vertexPath).string()
            << '\n';

        std::cout
            << "FRAGMENT SHADER:\n"
            << std::filesystem::absolute(fragmentPath).string()
            << '\n';

        // ----------------------------------------------------
        // 2. ABRIR LOS ARCHIVOS
        // ----------------------------------------------------

        std::ifstream vShaderFile(vertexPath);
        std::ifstream fShaderFile(fragmentPath);

        std::cout
            << "\nVertex encontrado: "
            << (vShaderFile.is_open() ? "SI" : "NO")
            << '\n';

        std::cout
            << "Fragment encontrado: "
            << (fShaderFile.is_open() ? "SI" : "NO")
            << '\n';

        std::cout
            << "====================================\n";

        if (!vShaderFile.is_open())
        {
            std::cerr
                << "ERROR: No se pudo abrir el vertex shader.\n";

            return;
        }

        if (!fShaderFile.is_open())
        {
            std::cerr
                << "ERROR: No se pudo abrir el fragment shader.\n";

            return;
        }

        // ----------------------------------------------------
        // 3. LEER EL CONTENIDO
        // ----------------------------------------------------

        std::stringstream vShaderStream;
        std::stringstream fShaderStream;

        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();

        vShaderFile.close();
        fShaderFile.close();

        std::cout
            << "Vertex shader: "
            << vertexCode.size()
            << " caracteres\n";

        std::cout
            << "Fragment shader: "
            << fragmentCode.size()
            << " caracteres\n";

        if (vertexCode.empty() || fragmentCode.empty())
        {
            std::cerr
                << "ERROR: Uno de los shaders esta vacio.\n";

            return;
        }

        // ----------------------------------------------------
        // 4. COMPILAR VERTEX SHADER
        // ----------------------------------------------------

        const GLchar* vShaderCode = vertexCode.c_str();

        GLuint vertex = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(
            vertex,
            1,
            &vShaderCode,
            nullptr
        );

        glCompileShader(vertex);

        GLint success = GL_FALSE;
        GLint logLength = 0;

        glGetShaderiv(
            vertex,
            GL_COMPILE_STATUS,
            &success
        );

        glGetShaderiv(
            vertex,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (!success)
        {
            std::vector<GLchar> infoLog(
                logLength > 1 ? logLength : 1024
            );

            glGetShaderInfoLog(
                vertex,
                static_cast<GLsizei>(infoLog.size()),
                nullptr,
                infoLog.data()
            );

            std::cerr
                << "\nERROR COMPILANDO VERTEX SHADER:\n"
                << infoLog.data()
                << '\n';

            glDeleteShader(vertex);
            return;
        }

        std::cout
            << "Vertex shader compilado correctamente.\n";

        // ----------------------------------------------------
        // 5. COMPILAR FRAGMENT SHADER
        // ----------------------------------------------------

        const GLchar* fShaderCode = fragmentCode.c_str();

        GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(
            fragment,
            1,
            &fShaderCode,
            nullptr
        );

        glCompileShader(fragment);

        glGetShaderiv(
            fragment,
            GL_COMPILE_STATUS,
            &success
        );

        glGetShaderiv(
            fragment,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (!success)
        {
            std::vector<GLchar> infoLog(
                logLength > 1 ? logLength : 1024
            );

            glGetShaderInfoLog(
                fragment,
                static_cast<GLsizei>(infoLog.size()),
                nullptr,
                infoLog.data()
            );

            std::cerr
                << "\nERROR COMPILANDO FRAGMENT SHADER:\n"
                << infoLog.data()
                << '\n';

            glDeleteShader(vertex);
            glDeleteShader(fragment);
            return;
        }

        std::cout
            << "Fragment shader compilado correctamente.\n";

        // ----------------------------------------------------
        // 6. CREAR Y ENLAZAR EL PROGRAMA
        // ----------------------------------------------------

        Program = glCreateProgram();

        glAttachShader(Program, vertex);
        glAttachShader(Program, fragment);

        glLinkProgram(Program);

        glGetProgramiv(
            Program,
            GL_LINK_STATUS,
            &success
        );

        glGetProgramiv(
            Program,
            GL_INFO_LOG_LENGTH,
            &logLength
        );

        if (!success)
        {
            std::vector<GLchar> infoLog(
                logLength > 1 ? logLength : 1024
            );

            glGetProgramInfoLog(
                Program,
                static_cast<GLsizei>(infoLog.size()),
                nullptr,
                infoLog.data()
            );

            std::cerr
                << "\nERROR ENLAZANDO SHADERS:\n"
                << infoLog.data()
                << '\n';

            glDeleteShader(vertex);
            glDeleteShader(fragment);
            glDeleteProgram(Program);

            Program = 0;
            return;
        }

        // ----------------------------------------------------
        // 7. OBTENER UNIFORM DE COLOR
        // ----------------------------------------------------

        uniformColor = glGetUniformLocation(
            Program,
            "color"
        );

        // ----------------------------------------------------
        // 8. LIBERAR SHADERS COMPILADOS
        // ----------------------------------------------------

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        std::cout
            << "Programa de shaders enlazado correctamente.\n"
            << "Shader listo para utilizarse.\n"
            << "====================================\n\n";
    }

    // --------------------------------------------------------
    // ACTIVAR SHADER
    // --------------------------------------------------------

    void Use()
    {
        if (Program != 0)
        {
            glUseProgram(Program);
        }
    }

    // --------------------------------------------------------
    // OBTENER UNIFORM DE COLOR
    // --------------------------------------------------------

    GLint getColorLocation()
    {
        return uniformColor;
    }
};

#endif
