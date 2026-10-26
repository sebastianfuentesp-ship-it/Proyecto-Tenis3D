
#include <iostream>
#include <vector>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Model.h"

// ============================================================
// CONFIGURACIÓN
// ============================================================

const unsigned int WIDTH = 1280;
const unsigned int HEIGHT = 720;

Camera camera(
    glm::vec3(0.0f, 5.2f, 11.5f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    -90.0f,
    -18.0f
);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool firstMouse = true;

float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

enum ModelId
{
    MODEL_MESA,
    MODEL_RECINTO,
    MODEL_GRADAS,
    MODEL_ESPECTADORES,
    MODEL_JUGADOR_ROJO,
    MODEL_JUGADOR_AZUL,
    MODEL_MARCADOR,
    MODEL_BANCA,
    MODEL_PALETA,
    MODEL_EQUIPAMIENTO_PALETA,
    MODEL_PELOTA,
    MODEL_EQUIPAMIENTO_PELOTAS,
    MODEL_BOTELLA,
    MODEL_MOCHILA,
    MODEL_TOALLA
};

struct ModelInstance
{
    ModelId modelId;
    glm::vec3 position;
    float scale;
    float rotationY;
};

glm::mat4 createModelTransform(
    const Model& model,
    const ModelInstance& instance
)
{
    const glm::vec3 center = model.GetCenter();

    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, instance.position);
    transform = glm::rotate(
        transform,
        glm::radians(instance.rotationY),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    transform = glm::scale(
        transform,
        glm::vec3(instance.scale)
    );
    transform = glm::translate(
        transform,
        glm::vec3(-center.x, -model.GetMinY(), -center.z)
    );

    return transform;
}

// ============================================================
// REDIMENSIONAR VENTANA
// ============================================================

void framebuffer_size_callback(
    GLFWwindow* window,
    int width,
    int height)
{
    glViewport(0, 0, width, height);
}

// ============================================================
// CONTROL DE CÁMARA CON EL RATÓN
// ============================================================

void mouse_callback(
    GLFWwindow* window,
    double xpos,
    double ypos)
{
    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset =
        static_cast<float>(xpos) - lastX;

    float yoffset =
        lastY - static_cast<float>(ypos);

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// ============================================================
// CONTROLES WASD
// ============================================================

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(FORWARD, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(LEFT, deltaTime);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        camera.ProcessKeyboard(RIGHT, deltaTime);
    }
}

// ============================================================
// MAIN
// ============================================================

int main()
{
    // --------------------------------------------------------
    // 1. INICIALIZAR GLFW
    // --------------------------------------------------------

    if (!glfwInit())
    {
        std::cerr
            << "ERROR: No se pudo inicializar GLFW.\n";

        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

    // --------------------------------------------------------
    // 2. CREAR VENTANA
    // --------------------------------------------------------

    GLFWwindow* window = glfwCreateWindow(
        WIDTH,
        HEIGHT,
        "Tenis de Mesa 3D",
        nullptr,
        nullptr
    );

    if (window == nullptr)
    {
        std::cerr
            << "ERROR: No se pudo crear la ventana.\n";

        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(
        window,
        framebuffer_size_callback
    );

    glfwSetCursorPosCallback(
        window,
        mouse_callback
    );

    glfwSetInputMode(
        window,
        GLFW_CURSOR,
        GLFW_CURSOR_DISABLED
    );

    // --------------------------------------------------------
    // 3. INICIALIZAR GLEW
    // --------------------------------------------------------

    glewExperimental = GL_TRUE;

    GLenum estadoGLEW = glewInit();

    if (estadoGLEW != GLEW_OK)
    {
        std::cerr
            << "ERROR GLEW: "
            << glewGetErrorString(estadoGLEW)
            << '\n';

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }

    // Limpiar errores iniciales de GLEW.
    while (glGetError() != GL_NO_ERROR)
    {
    }

    glViewport(0, 0, WIDTH, HEIGHT);

    // --------------------------------------------------------
    // 4. CONFIGURAR OPENGL
    // --------------------------------------------------------

    glEnable(GL_DEPTH_TEST);

    // Eliminar caras posteriores para optimizar el renderizado.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // --------------------------------------------------------
    // 5. CARGAR SHADERS
    // --------------------------------------------------------

    std::cout
        << "\n====================================\n"
        << "INICIANDO TENIS DE MESA 3D\n"
        << "====================================\n";

    Shader shader(
        "Shader/tenis.vs",
        "Shader/tenis.frag"
    );

    if (shader.Program == 0)
    {
        std::cerr
            << "ERROR: No se pudo crear el programa shader.\n";

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }

    // --------------------------------------------------------
    // 6. CARGAR LOS MODELOS OBJ PARA LA ESCENA DE PRUEBA
    // --------------------------------------------------------

    const char* modelPaths[] =
    {
        "Models/MesaTenis.obj",
        "Models/recinto.obj",
        "Models/gradas.obj",
        "Models/espectadores.obj",
        "Models/jugador_rojo.obj",
        "Models/jugador_azul.obj",
        "Models/marcador.obj",
        "Models/banca.obj",
        "Models/paleta.obj",
        "Models/equipamiento_paleta.obj",
        "Models/pelota.obj",
        "Models/equipamiento_pelotas.obj",
        "Models/botella.obj",
        "Models/mochila.obj",
        "Models/toalla.obj"
    };

    const size_t modelCount = sizeof(modelPaths) / sizeof(modelPaths[0]);
    std::vector<Model> models;
    models.reserve(modelCount);

    for (size_t i = 0; i < modelCount; ++i)
    {
        const char* path = modelPaths[i];
        std::cout << "\nCargando para OpenGL: " << path << '\n';
        models.emplace_back(
            const_cast<GLchar*>(path),
            i == MODEL_MESA
        );
    }

    // Las tres áreas ocupan el recinto completo sin invadir la mesa.
    std::vector<ModelInstance> sceneInstances =
    {
        // El piso del recinto llega hasta Y=0 en el OBJ; se dibuja primero.
        { MODEL_RECINTO,          glm::vec3(0.0f, -0.36f, 0.0f), 1.0f, 0.0f },

        // Área de competencia
        { MODEL_MESA,             glm::vec3(0.0f, 0.0f,   0.0f), 1.0f,   0.0f },
        { MODEL_JUGADOR_ROJO,     glm::vec3(2.0f, 0.0f,   0.0f), 1.0f, -90.0f },
        { MODEL_JUGADOR_AZUL,     glm::vec3(-2.0f, 0.0f,  0.0f), 1.0f,  90.0f },
        { MODEL_MARCADOR,         glm::vec3(0.0f, 0.0f,  -3.8f), 1.0f,   0.0f },
        { MODEL_PELOTA,           glm::vec3(0.0f, 0.85f,  0.0f), 1.0f,   0.0f },
        { MODEL_PALETA,           glm::vec3(-0.8f, 0.85f, 0.35f), 1.0f, 0.0f },
        { MODEL_PALETA,           glm::vec3(0.8f, 0.85f, -0.35f), 1.0f, 180.0f },

        // Área de espectadores: gradas en el fondo y en ambos costados.
        { MODEL_GRADAS,           glm::vec3(-4.0f, 0.0f, -8.4f), 1.0f,   0.0f },
        { MODEL_GRADAS,           glm::vec3(0.0f, 0.0f,  -8.4f), 1.0f,   0.0f },
        { MODEL_GRADAS,           glm::vec3(4.0f, 0.0f,  -8.4f), 1.0f,   0.0f },
        { MODEL_GRADAS,           glm::vec3(-7.5f, 0.0f, -4.0f), 1.0f,  90.0f },
        { MODEL_GRADAS,           glm::vec3(-7.5f, 0.0f,  0.0f), 1.0f,  90.0f },
        { MODEL_GRADAS,           glm::vec3(-7.5f, 0.0f,  4.0f), 1.0f,  90.0f },
        { MODEL_GRADAS,           glm::vec3(7.5f, 0.0f, -4.0f), 1.0f, -90.0f },
        { MODEL_GRADAS,           glm::vec3(7.5f, 0.0f,  0.0f), 1.0f, -90.0f },
        { MODEL_GRADAS,           glm::vec3(7.5f, 0.0f,  4.0f), 1.0f, -90.0f },
        // Área de descanso y equipamiento, visible desde la entrada.
        { MODEL_BANCA,             glm::vec3(-4.5f, 0.0f, 4.8f), 1.0f,  90.0f },
        { MODEL_BANCA,             glm::vec3(4.5f, 0.0f,  4.8f), 1.0f, -90.0f },
        { MODEL_MOCHILA,           glm::vec3(-3.4f, 0.0f, 5.7f), 1.0f, 0.0f },
        { MODEL_MOCHILA,           glm::vec3(3.4f, 0.0f,  5.7f), 1.0f, 180.0f },
        { MODEL_BOTELLA,           glm::vec3(-5.4f, 0.0f, 5.6f), 1.0f, 0.0f },
        { MODEL_BOTELLA,           glm::vec3(5.4f, 0.0f,  5.6f), 1.0f, 0.0f },
        { MODEL_TOALLA,            glm::vec3(-4.5f, 0.0f, 5.5f), 1.0f, 0.0f },
        { MODEL_TOALLA,            glm::vec3(4.5f, 0.0f,  5.5f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PALETA, glm::vec3(-5.0f, 0.0f, 2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PALETA, glm::vec3(5.0f, 0.0f,  2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PELOTAS, glm::vec3(-4.0f, 0.0f, 2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PELOTAS, glm::vec3(4.0f, 0.0f,  2.8f), 1.0f, 0.0f },

    };

    // Tres filas por cada módulo: las filas suben conforme se alejan de la cancha.
    const float rowHeights[] = { 0.18f, 0.53f, 0.88f };
    const float backRowZ[] = { -7.55f, -8.40f, -9.25f };
    const float sideRowOffset[] = { -0.85f, 0.0f, 0.85f };
    const float audienceColumns[] = { -4.0f, 0.0f, 4.0f };
    const float sideSections[] = { -4.0f, 0.0f, 4.0f };

    for (size_t row = 0; row < 3; ++row)
    {
        for (float x : audienceColumns)
        {
            sceneInstances.push_back(
                { MODEL_ESPECTADORES,
                    glm::vec3(x, rowHeights[row], backRowZ[row]),
                    1.0f, 0.0f }
            );
        }

        for (float z : sideSections)
        {
            sceneInstances.push_back(
                { MODEL_ESPECTADORES,
                    glm::vec3(-7.5f - sideRowOffset[row], rowHeights[row], z),
                    1.0f, 90.0f }
            );
            sceneInstances.push_back(
                { MODEL_ESPECTADORES,
                    glm::vec3(7.5f + sideRowOffset[row], rowHeights[row], z),
                    1.0f, -90.0f }
            );
        }
    }

    std::cout << "\nInstancias distribuidas en 3 zonas: "
        << sceneInstances.size() << '\n';

    std::cout
        << "\nModelos listos para dibujar: " << models.size() << '\n';

    // --------------------------------------------------------
    // 7. BUCLE PRINCIPAL
    // --------------------------------------------------------

    while (!glfwWindowShouldClose(window))
    {
        // Calcular tiempo entre fotogramas.
        float currentFrame =
            static_cast<float>(glfwGetTime());

        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Leer teclado.
        processInput(window);

        // Limpiar pantalla y profundidad.
        glClearColor(
            0.08f,
            0.10f,
            0.14f,
            1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        // Activar shader.
        shader.Use();

        // ----------------------------------------------------
        // 8. OBTENER DIMENSIONES DE LA VENTANA
        // ----------------------------------------------------

        int width = 0;
        int height = 0;

        glfwGetFramebufferSize(
            window,
            &width,
            &height
        );

        if (width == 0 || height == 0)
        {
            glfwPollEvents();
            continue;
        }

        // ----------------------------------------------------
        // 9. MATRIZ DE PROYECCIÓN
        // ----------------------------------------------------

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.GetZoom()),
            static_cast<float>(width) /
            static_cast<float>(height),
            0.1f,
            1000.0f
        );

        glUniformMatrix4fv(
            glGetUniformLocation(
                shader.Program,
                "projection"
            ),
            1,
            GL_FALSE,
            &projection[0][0]
        );

        // ----------------------------------------------------
        // 10. MATRIZ DE VISTA
        // ----------------------------------------------------

        glm::mat4 view = camera.GetViewMatrix();

        glUniformMatrix4fv(
            glGetUniformLocation(
                shader.Program,
                "view"
            ),
            1,
            GL_FALSE,
            &view[0][0]
        );

        // ----------------------------------------------------
        // 11. DIBUJAR TODOS LOS MODELOS OBJ
        // ----------------------------------------------------

        const GLint modelLocation = glGetUniformLocation(
            shader.Program,
            "model"
        );

        for (const ModelInstance& instance : sceneInstances)
        {
            const glm::mat4 modelTransform = createModelTransform(
                models[instance.modelId],
                instance
            );
            glUniformMatrix4fv(
                modelLocation,
                1,
                GL_FALSE,
                &modelTransform[0][0]
            );
            models[instance.modelId].Draw(shader);
        }

        // ----------------------------------------------------
        // 13. ACTUALIZAR VENTANA
        // ----------------------------------------------------

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --------------------------------------------------------
    // 14. LIBERAR RECURSOS
    // --------------------------------------------------------

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout
        << "\nAplicacion cerrada correctamente.\n";

    return 0;
}
