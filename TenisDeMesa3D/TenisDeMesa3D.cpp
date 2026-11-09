// ============================================================
// PROYECTO 3D: TENIS DE MESA EN OPENGL
// ============================================================
// Descripción: Escenario 3D interactivo con iluminación multicolor dinámica,
//              sistema de cámaras (Libre y Orbital TV), animación cinemática
//              articulada de atletas y espectadores, simulación de partido de
//              tenis de mesa con marcador digital y motor de keyframes.
// ============================================================

#include <iostream>
#include <vector>
#include <cmath>

// Vinculación estática de GLEW antes de incluir sus cabeceras
#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif

#include <GL/glew.h>      // Gestión de extensiones de OpenGL
#include <GLFW/glfw3.h>   // Creación de ventana y manejo de eventos/entradas

#include <glm/glm.hpp>                  // Matemáticas de vectores y matrices (GLM)
#include <glm/gtc/matrix_transform.hpp> // Transformaciones (translate, rotate, scale, lookAt, perspective)

// Módulos y cabeceras personalizadas del proyecto
#include "Shader.h"     // Carga, compilación y vinculación de Shaders (VS y FS)
#include "Camera.h"     // Control de navegación de cámara en 1ra/3ra persona
#include "Model.h"      // Cargador y renderizador de modelos 3D en formato OBJ
#include "Animacion.h"  // Motor cinemático del partido (pelota, atletas y reglas)
#include "Marcador.h"   // Visualización dinámica del marcador digital de 7 segmentos
#include "Keyframes.h"  // Grabación, almacenamiento e interpolación de keyframes

// ============================================================
// CONFIGURACIÓN GLOBAL Y VARIABLES DE ESTADO
// ============================================================

// Dimensiones por defecto de la ventana de renderizado
const unsigned int WIDTH = 1280;
const unsigned int HEIGHT = 720;

// Instancia global de la cámara libre (Posición XYZ, Vector Arriba, Yaw, Pitch)
Camera camera(
    glm::vec3(0.0f, 5.2f, 11.5f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    -90.0f,
    -18.0f
);

// Control de tiempo por fotograma (Delta Time)
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Control de eventos del ratón
bool firstMouse = true;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

// ------------------------------------------------------------
// ESTADOS DE ILUMINACIÓN (Control con tecla 'L')
// ------------------------------------------------------------
// 0: Apagado | 1: Blanco Cálido | 2: Azul Neón | 3: Rojo Escarlata | 4: Reflector Cónico Protagonista
int lightColorMode = 1;
bool lKeyPressed = false; // Bandera anti-rebote para tecla L

// ------------------------------------------------------------
// ESTADOS DE CÁMARA (Control con tecla 'C')
// ------------------------------------------------------------
// false: Cámara Libre (WASD + Mouse) | true: Cámara Orbital TV (Giro 360° automático)
bool isOrbitalCamera = false;
bool cKeyPressed = false; // Bandera anti-rebote para tecla C

// ------------------------------------------------------------
// LUCES AUXILIARES INDEPENDIENTES
// ------------------------------------------------------------
bool spotLightOn = false;  // Reflector cónico manual [F]
bool dirLightOn = false;   // Luz direccional tipo sol [G]

// ------------------------------------------------------------
// SISTEMA DE ANIMACIÓN POR KEYFRAMES (Partido + Luces)
// ------------------------------------------------------------
KeyframeAnimator keyframes;
const char* kKeyframeFile = "keyframes.txt"; // Archivo para persisitir los keyframes guardados
bool kfSaveRequested = false;
bool kfPlayRequested = false;
bool kfUndoRequested = false;
bool kfClearRequested = false;

// Función de detección de pulso único de tecla (evita múltiples detecciones por pulsación)
static bool justPressed(GLFWwindow* window, int key)
{
    static bool wasDown[GLFW_KEY_LAST + 1] = {};
    const bool down = glfwGetKey(window, key) == GLFW_PRESS;
    const bool pressed = down && !wasDown[key];
    wasDown[key] = down;
    return pressed;
}

// ============================================================
// ESTRUCTURAS Y ENUMERACIONES DE MODELOS 3D
// ============================================================

// Identificadores únicos para cada modelo cargado en la GPU
enum ModelId
{
    MODEL_MESA,
    MODEL_RECINTO,
    MODEL_GRADAS,
    MODEL_MARCADOR,
    MODEL_BANCA,
    MODEL_PALETA,
    MODEL_EQUIPAMIENTO_PALETA,
    MODEL_PELOTA,
    MODEL_EQUIPAMIENTO_PELOTAS,
    MODEL_BOTELLA,
    MODEL_MOCHILA,
    MODEL_TOALLA,
    MODEL_ESPECTADORES_CUERPO,
    MODEL_ESPECTADORES_BRAZOS,

    // Partes separadas para la articulación por Skinning de los atletas
    MODEL_JUGADOR_ROJO_CUERPO,
    MODEL_JUGADOR_ROJO_BRAZO,
    MODEL_JUGADOR_ROJO_CABEZA,

    MODEL_JUGADOR_AZUL_CUERPO,
    MODEL_JUGADOR_AZUL_BRAZO,
    MODEL_JUGADOR_AZUL_CABEZA
};

// Estructura de instanciación de modelos en el mundo 3D
struct ModelInstance
{
    ModelId modelId;       // ID del modelo base
    glm::vec3 position;    // Posición XYZ en el espacio del mundo
    float scale;           // Factor de escala uniforme
    float rotationY;       // Ángulo de rotación sobre el eje Y (grados)
};

// Declaración previa de funciones auxiliares y callbacks
static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void processInput(GLFWwindow* window);
static glm::mat4 createModelTransformCustom(const glm::vec3& center, float minY, const ModelInstance& instance);
static glm::mat4 createModelTransform(const Model& model, const ModelInstance& instance);

// ============================================================
// TRANSFORMACIONES MATRICIALES AUXILIARES
// ============================================================

// Construye la matriz de modelo alineando el origen con la base (minY) y centro geométrico del objeto
static glm::mat4 createModelTransformCustom(
    const glm::vec3& center,
    float minY,
    const ModelInstance& instance
)
{
    glm::mat4 transform(1.0f);
    // 1. Posicionamiento en el mundo
    transform = glm::translate(transform, instance.position);
    // 2. Orientación sobre el eje Y
    transform = glm::rotate(
        transform,
        glm::radians(instance.rotationY),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    // 3. Escalado uniforme del modelo
    transform = glm::scale(
        transform,
        glm::vec3(instance.scale)
    );
    // 4. Centrado del pivote en la caja delimitadora (Bounding Box)
    transform = glm::translate(
        transform,
        glm::vec3(-center.x, -minY, -center.z)
    );

    return transform;
}

// Sobrecarga de conveniencia directa a partir del objeto Model
static glm::mat4 createModelTransform(
    const Model& model,
    const ModelInstance& instance
)
{
    return createModelTransformCustom(model.GetCenter(), model.GetMinY(), instance);
}

// Callback de redimensionamiento de ventana
static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Callback para la orientación de la cámara libre con el ratón
static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Ignorar en cámara orbital para mantener el ratón disociado
    if (isOrbitalCamera)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        return;
    }

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos); // Coordenadas Y invertidas

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// Procesamiento de comandos de teclado por cuadro
static void processInput(GLFWwindow* window)
{
    // Salida de la aplicación
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    // Desplazamiento de cámara libre (WASD)
    if (!isOrbitalCamera)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
    }

    // Alternar modos de luz (Tecla L)
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
    {
        if (!lKeyPressed)
        {
            if (!keyframes.IsPlaying())
                lightColorMode = (lightColorMode + 1) % 5;
            lKeyPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_RELEASE)
    {
        lKeyPressed = false;
    }

    // Alternar modos de cámara (Tecla C)
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
    {
        if (!cKeyPressed)
        {
            isOrbitalCamera = !isOrbitalCamera;
            cKeyPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
    {
        cKeyPressed = false;
    }

    // Control manual de reflector y luz direccional (Teclas F y G)
    if (justPressed(window, GLFW_KEY_F) && !keyframes.IsPlaying())
    {
        spotLightOn = !spotLightOn;
    }
    if (justPressed(window, GLFW_KEY_G) && !keyframes.IsPlaying())
    {
        dirLightOn = !dirLightOn;
    }

    // Peticiones de acciones sobre la animación por Keyframes
    if (justPressed(window, GLFW_KEY_K) && !keyframes.IsPlaying()) kfSaveRequested = true;
    if (justPressed(window, GLFW_KEY_P)) kfPlayRequested = true;
    if (justPressed(window, GLFW_KEY_U) && !keyframes.IsPlaying()) kfUndoRequested = true;
    if (justPressed(window, GLFW_KEY_DELETE) && !keyframes.IsPlaying()) kfClearRequested = true;

    // Velocidad de reproducción de keyframes ([ y ])
    if (justPressed(window, GLFW_KEY_LEFT_BRACKET))
    {
        keyframes.speed = std::max(0.25f, keyframes.speed / 1.25f);
    }
    if (justPressed(window, GLFW_KEY_RIGHT_BRACKET))
    {
        keyframes.speed = std::min(4.0f, keyframes.speed * 1.25f);
    }
}

// ============================================================
// FUNCIÓN PRINCIPAL (MAIN)
// ============================================================

int main()
{
    // 1. INICIALIZACIÓN Y CONFIGURACIÓN DE GLFW
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Tenis de Mesa 3D - Proyecto Final", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 2. INICIALIZACIÓN DE GLEW Y ESTADOS DE OPENGL
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return -1;

    // Limpiar errores iniciales producidos por GLEW
    while (glGetError() != GL_NO_ERROR) {}

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    std::cout << "\n====================================\n"
        << "  INICIANDO TENIS DE MESA 3D\n"
        << "====================================\n"
        << " CONTROLES:\n"
        << " [WASD] : Mover Cámara Libre\n"
        << " [Mouse]: Orientar vista de Cámara Libre\n"
        << " [L]    : Cambiar Modos de Luz (Modo 4 = Reflector Cónico Destacado)\n"
        << " [C]    : Alternar entre Cámara Libre y Cámara Orbital (TV)\n"
        << " [F]    : Encender / Apagar Reflector Cónico manual\n"
        << " [G]    : Encender / Apagar Luz Direccional\n"
        << " [K]    : Guardar instante de partido como Keyframe\n"
        << " [P]    : Reproducir / Detener secuencia de Keyframes\n"
        << " [U]    : Deshacer último Keyframe | [Supr]: Borrar todos\n"
        << " [ y ]  : Bajar / Subir velocidad de keyframes\n"
        << " [ESC]  : Salir de la aplicación\n"
        << "====================================\n";

    // 3. CARGA DE SHADERS Y KEYFRAMES PERSISTIDOS
    Shader shader("Shader/tenis.vs", "Shader/tenis.frag");
    if (shader.Program == 0) return -1;

    if (keyframes.Load(kKeyframeFile))
    {
        std::cout << " Keyframes cargados desde el archivo: " << keyframes.Count() << "\n";
    }

    // 4. RUTAS DE MODELOS 3D (OBJ)
    const char* modelPaths[] =
    {
        "Models/MesaTenis.obj",
        "Models/recinto.obj",
        "Models/gradas.obj",
        "Models/marcador.obj",
        "Models/banca.obj",
        "Models/paleta.obj",
        "Models/equipamiento_paleta.obj",
        "Models/pelota.obj",
        "Models/equipamiento_pelotas.obj",
        "Models/botella.obj",
        "Models/mochila.obj",
        "Models/toalla.obj",
        "Models/Cuerpo_Espectadores.obj",
        "Models/Brazos_Espectadores.obj",
        "Models/jugador_rojo_cuerpo.obj",
        "Models/jugador_rojo_brazo.obj",
        "Models/jugador_rojo_cabeza.obj",
        "Models/jugador_azul_cuerpo.obj",
        "Models/jugador_azul_brazo.obj",
        "Models/jugador_azul_cabeza.obj"
    };

    const size_t modelCount = sizeof(modelPaths) / sizeof(modelPaths[0]);
    std::vector<Model> models;
    models.reserve(modelCount);

    for (size_t i = 0; i < modelCount; ++i)
    {
        models.emplace_back(const_cast<GLchar*>(modelPaths[i]), i == MODEL_MESA);
    }

    // 5. DISTRIBUCIÓN DE INSTANCIAS DE OBJETOS EN EL ESCENARIO
    std::vector<ModelInstance> sceneInstances =
    {
        { MODEL_RECINTO,              glm::vec3(0.0f, 0.0f,   0.0f), 1.0f, 0.0f },
        { MODEL_MESA,                 glm::vec3(0.0f, 0.0f,   0.0f), 1.0f, 0.0f },
        { MODEL_MARCADOR,             glm::vec3(0.0f, 0.0f,  -3.8f), 1.0f, 0.0f },
        { MODEL_PELOTA,               glm::vec3(0.0f, 0.85f,  0.0f), 1.0f, 0.0f },
        { MODEL_GRADAS,              glm::vec3(-4.0f, 0.0f, -8.4f), 1.0f, 0.0f },
        { MODEL_GRADAS,              glm::vec3(0.0f, 0.0f,  -8.4f), 1.0f, 0.0f },
        { MODEL_GRADAS,              glm::vec3(4.0f, 0.0f,  -8.4f), 1.0f, 0.0f },
        { MODEL_GRADAS,              glm::vec3(-7.5f, 0.0f, -4.0f), 1.0f, 90.0f },
        { MODEL_GRADAS,              glm::vec3(-7.5f, 0.0f,  0.0f), 1.0f, 90.0f },
        { MODEL_GRADAS,              glm::vec3(-7.5f, 0.0f,  4.0f), 1.0f, 90.0f },
        { MODEL_GRADAS,              glm::vec3(7.5f, 0.0f, -4.0f), 1.0f, -90.0f },
        { MODEL_GRADAS,              glm::vec3(7.5f, 0.0f,   0.0f), 1.0f, -90.0f },
        { MODEL_GRADAS,              glm::vec3(7.5f, 0.0f,   4.0f), 1.0f, -90.0f },
        { MODEL_BANCA,                glm::vec3(-4.5f, 0.0f, 4.8f), 1.0f, 90.0f },
        { MODEL_BANCA,                glm::vec3(4.5f, 0.0f,  4.8f), 1.0f, -90.0f },
        { MODEL_MOCHILA,              glm::vec3(-3.4f, 0.0f, 5.7f), 1.0f, 0.0f },
        { MODEL_MOCHILA,              glm::vec3(3.4f, 0.0f,  5.7f), 1.0f, 180.0f },
        { MODEL_BOTELLA,              glm::vec3(-5.4f, 0.0f, 5.6f), 1.0f, 0.0f },
        { MODEL_BOTELLA,              glm::vec3(5.4f, 0.0f,  5.6f), 1.0f, 0.0f },
        { MODEL_TOALLA,               glm::vec3(-4.5f, 0.0f, 6.00f), 1.0f, 0.0f },
        { MODEL_TOALLA,               glm::vec3(4.5f, 0.0f,  6.00f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PALETA, glm::vec3(-5.0f, 0.0f, 2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PALETA, glm::vec3(5.0f, 0.0f,  2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PELOTAS,glm::vec3(-4.0f, 0.0f, 2.8f), 1.0f, 0.0f },
        { MODEL_EQUIPAMIENTO_PELOTAS,glm::vec3(4.0f, 0.0f,  2.8f), 1.0f, 0.0f }
    };

    // Generación de la cuadrícula de espectadores sentados en gradas traseras y laterales
    const float spectatorScale = 1.0f;
    const float rowLocalZ[] = { 1.14f, 0.42f, -0.36f };
    const float rowFeetY[] = { 0.0f, 0.364f, 0.684f };
    const float standCenters[] = { -4.0f, 0.0f, 4.0f };
    const float backStandZ = -8.4f;
    const float sideStandX = 7.5f;

    for (size_t row = 0; row < 3; ++row)
    {
        for (float c : standCenters)
        {
            // Espectadores de fondo
            const glm::vec3 backPos(c, rowFeetY[row], backStandZ + rowLocalZ[row]);
            sceneInstances.push_back({ MODEL_ESPECTADORES_CUERPO, backPos, spectatorScale, 0.0f });
            sceneInstances.push_back({ MODEL_ESPECTADORES_BRAZOS, backPos, spectatorScale, 0.0f });

            // Espectadores del lateral izquierdo
            const glm::vec3 leftPos(-sideStandX + rowLocalZ[row], rowFeetY[row], c);
            sceneInstances.push_back({ MODEL_ESPECTADORES_CUERPO, leftPos, spectatorScale, 90.0f });
            sceneInstances.push_back({ MODEL_ESPECTADORES_BRAZOS, leftPos, spectatorScale, 90.0f });

            // Espectadores del lateral derecho
            const glm::vec3 rightPos(sideStandX - rowLocalZ[row], rowFeetY[row], c);
            sceneInstances.push_back({ MODEL_ESPECTADORES_CUERPO, rightPos, spectatorScale, -90.0f });
            sceneInstances.push_back({ MODEL_ESPECTADORES_BRAZOS, rightPos, spectatorScale, -90.0f });
        }
    }

    // 6. UBICACIÓN DE UNIFORMS EN EL SHADER
    const GLint projectionLoc = glGetUniformLocation(shader.Program, "projection");
    const GLint viewLoc = glGetUniformLocation(shader.Program, "view");
    const GLint modelLocation = glGetUniformLocation(shader.Program, "model");
    const GLint objectColorLoc = glGetUniformLocation(shader.Program, "objectColor");
    const GLint viewPosLoc = glGetUniformLocation(shader.Program, "viewPos");

    // Arreglo de 12 lámparas omnidireccionales en el techo del estadio
    constexpr int kNumCeilingLights = 12;
    glm::vec3 ceilingLights[kNumCeilingLights];
    {
        const float lightX[3] = { -5.2f, 0.0f, 5.2f };
        const float lightZ[4] = { 8.0f, 2.7f, -2.7f, -8.0f };
        const float lightY = 6.55f;
        int n = 0;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 3; ++col)
                ceilingLights[n++] = glm::vec3(lightX[col], lightY, lightZ[row]);
    }
    const GLint lightPositionsLoc = glGetUniformLocation(shader.Program, "lightPositions[0]");
    const GLint lightColorLoc = glGetUniformLocation(shader.Program, "lightColor");

    // Luz direccional sol/ambiente
    const GLint dirLightDirectionLoc = glGetUniformLocation(shader.Program, "dirLightDirection");
    const GLint dirLightColorLoc = glGetUniformLocation(shader.Program, "dirLightColor");
    const glm::vec3 dirLightDirection = glm::normalize(glm::vec3(-0.35f, -1.0f, -0.25f));
    const glm::vec3 dirLightColor(0.50f, 0.47f, 0.40f);

    // Reflector cónico apuntando directamente al centro de la mesa
    const GLint spotPositionLoc = glGetUniformLocation(shader.Program, "spotPosition");
    const GLint spotDirectionLoc = glGetUniformLocation(shader.Program, "spotDirection");
    const GLint spotInnerCosLoc = glGetUniformLocation(shader.Program, "spotInnerCos");
    const GLint spotOuterCosLoc = glGetUniformLocation(shader.Program, "spotOuterCos");
    const GLint spotColorLoc = glGetUniformLocation(shader.Program, "spotColor");
    const glm::vec3 spotPosition(0.0f, 6.5f, 0.0f);
    const glm::vec3 spotTarget(0.0f, 0.85f, 0.0f);
    const glm::vec3 spotDirection = glm::normalize(spotTarget - spotPosition);
    const float spotInnerCos = std::cos(glm::radians(18.0f));
    const float spotOuterCos = std::cos(glm::radians(30.0f));
    const glm::vec3 spotColor(1.20f * 1.00f, 1.20f * 0.97f, 1.20f * 0.90f);

    // Uniforms de Skinning cinemático para la malla de los jugadores
    const GLint skinModeLoc = glGetUniformLocation(shader.Program, "skinMode");
    const GLint boneALoc = glGetUniformLocation(shader.Program, "boneA");
    const GLint boneBLoc = glGetUniformLocation(shader.Program, "boneB");
    const GLint boneCLoc = glGetUniformLocation(shader.Program, "boneC");
    const GLint skinP0Loc = glGetUniformLocation(shader.Program, "skinP0");
    const GLint skinP1Loc = glGetUniformLocation(shader.Program, "skinP1");

    // Configuración física de los jugadores en la mesa
    const float playerBaseZ = 0.20f;
    const Anim::PlayerSetup jugadorRojo = {
        1.0f, glm::vec3(2.0f, 0.0f, playerBaseZ), -90.0f,
        models[MODEL_JUGADOR_ROJO_CUERPO].GetCenter(), models[MODEL_JUGADOR_ROJO_CUERPO].GetMinY()
    };

    const Anim::PlayerSetup jugadorAzul = {
        -1.0f, glm::vec3(-2.0f, 0.0f, playerBaseZ), 90.0f,
        models[MODEL_JUGADOR_AZUL_CUERPO].GetCenter(), models[MODEL_JUGADOR_AZUL_CUERPO].GetMinY()
    };

    // Inicializar simulación del partido completo
    Anim::Match match;
    match.build(jugadorRojo, jugadorAzul);

    // Inicializar marcador digital de 7 segmentos
    MarcadorDigital marcador;
    marcador.Init();

    glm::mat4 marcadorTransform(1.0f);
    for (const ModelInstance& instance : sceneInstances)
    {
        if (instance.modelId == MODEL_MARCADOR)
        {
            marcadorTransform = createModelTransform(models[MODEL_MARCADOR], instance);
            break;
        }
    }

    // Funciones lambda locales de ayuda de renderizado
    auto setMat = [](GLint loc, const glm::mat4& m)
        {
            glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
        };

    auto drawPlayer = [&](const Anim::PlayerSetup& setup,
        const Anim::PlayerPose& pose,
        ModelId idCuerpo, ModelId idBrazo, ModelId idCabeza,
        const glm::vec3& tinte)
        {
            const float m = setup.m;

            // Render del Tronco y Piernas (Skinning 1)
            glUniform1i(skinModeLoc, 1);
            setMat(boneALoc, pose.boneLegs);
            setMat(boneBLoc, pose.boneTorso);
            setMat(boneCLoc, pose.boneFreeArm);
            glUniform4f(skinP0Loc, 0.80f, 1.05f, Anim::kTorsoAxisX * m, m);

            glUniform3f(objectColorLoc, tinte.x, tinte.y, tinte.z);
            setMat(modelLocation, pose.root);
            models[idCuerpo].Draw(shader);

            // Render del Brazo Hábil (Skinning 2)
            const glm::vec3 codo = Anim::mirrorX(Anim::kElbow, m);
            const glm::vec3 eje = glm::normalize(Anim::mirrorX(Anim::kHand, m) - codo);
            glUniform1i(skinModeLoc, 2);
            setMat(boneALoc, pose.boneUpper);
            setMat(boneBLoc, pose.boneFore);
            glUniform4f(skinP0Loc, codo.x, codo.y, codo.z, 0.05f);
            glUniform4f(skinP1Loc, eje.x, eje.y, eje.z, 0.0f);

            glUniform3f(objectColorLoc, tinte.x, tinte.y, tinte.z);
            setMat(modelLocation, pose.root);
            models[idBrazo].Draw(shader);

            // Render de Cabeza y Raqueta (Objetos Rígidos acoplados)
            glUniform1i(skinModeLoc, 0);
            glUniform3f(objectColorLoc, 1.0f, 1.0f, 1.0f);
            setMat(modelLocation, pose.root * pose.headLocal);
            models[idCabeza].Draw(shader);

            setMat(modelLocation, pose.root * pose.racketLocal);
            models[MODEL_PALETA].Draw(shader);
        };

    // ============================================================
    // BUCLE PRINCIPAL DE RENDERIZADO (GAME LOOP)
    // ============================================================
    while (!glfwWindowShouldClose(window))
    {
        // Delta Time entre fotogramas
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.Use();

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);

        if (width == 0 || height == 0)
        {
            glfwPollEvents();
            continue;
        }

        // ------------------------------------------------------------
        // MATRICES DE CÁMARA (LIBRE U ORBITAL INDEPENDIENTE)
        // ------------------------------------------------------------
        glm::mat4 view;
        glm::vec3 currentCamPos;

        if (isOrbitalCamera)
        {
            // Cámara orbital rotatoria alrededor del centro de la mesa
            float radius = 7.5f;
            float camHeight = 4.2f;
            float orbitalSpeed = 0.35f;
            float angle = currentFrame * orbitalSpeed;

            currentCamPos = glm::vec3(radius * std::cos(angle), camHeight, radius * std::sin(angle));
            glm::vec3 target(0.0f, 0.85f, 0.0f);
            view = glm::lookAt(currentCamPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
        }
        else
        {
            // Cámara libre navegable con WASD + ratón
            view = camera.GetViewMatrix();
            currentCamPos = camera.GetPosition();
        }

        // ------------------------------------------------------------
        // MOTOR DE KEYFRAMES (Controlling partido e iluminación)
        // ------------------------------------------------------------
        float activeMatchTime = match.localTime(currentFrame);
        const AnimKeyframe liveState = KeyframeAnimator::Capture(activeMatchTime, lightColorMode, spotLightOn, dirLightOn);

        if (kfSaveRequested)
        {
            keyframes.Add(liveState);
            keyframes.Save(kKeyframeFile);
            std::cout << " Keyframe #" << keyframes.Count() << " guardado (tiempo del partido = " << activeMatchTime << "s)\n";
        }
        if (kfUndoRequested)
        {
            if (keyframes.RemoveLast())
            {
                keyframes.Save(kKeyframeFile);
                std::cout << " Último keyframe borrado. Quedan " << keyframes.Count() << '\n';
            }
        }
        if (kfClearRequested)
        {
            keyframes.Clear();
            keyframes.Save(kKeyframeFile);
            std::cout << " Todos los keyframes han sido borrados.\n";
        }

        static int backupLightMode = 1;
        static bool backupSpot = false, backupDir = false;

        if (kfPlayRequested)
        {
            if (keyframes.IsPlaying())
            {
                keyframes.Stop();
                lightColorMode = backupLightMode; spotLightOn = backupSpot; dirLightOn = backupDir;
                std::cout << " Reproducción de animación detenida.\n";
            }
            else if (keyframes.Count() == 0)
            {
                std::cout << " No hay keyframes guardados. Presiona [K] para guardar el instante del partido.\n";
            }
            else
            {
                backupLightMode = lightColorMode; backupSpot = spotLightOn; backupDir = dirLightOn;
                keyframes.Play(liveState);
                std::cout << " Reproduciendo secuencia de keyframes (" << keyframes.Count() << " frames)...\n";
            }
        }
        kfSaveRequested = kfPlayRequested = kfUndoRequested = kfClearRequested = false;

        if (keyframes.IsPlaying())
        {
            // Los keyframes evalúan de forma interpolada el tiempo del partido y el estado de las luces
            const AnimKeyframe st = keyframes.Update(deltaTime);
            activeMatchTime = st.animTime;
            lightColorMode = st.lightMode;
            spotLightOn = (st.spotOn != 0);
            dirLightOn = (st.dirOn != 0);

            if (!keyframes.IsPlaying())
            {
                lightColorMode = backupLightMode; spotLightOn = backupSpot; dirLightOn = backupDir;
                std::cout << " Reproducción de keyframes finalizada.\n";
            }
        }

        // Matriz de Proyección Perspectiva
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.GetZoom()),
            static_cast<float>(width) / static_cast<float>(height),
            0.1f, 1000.0f
        );

        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
        glUniform3f(viewPosLoc, currentCamPos.x, currentCamPos.y, currentCamPos.z);

        // ------------------------------------------------------------
        // CÁLCULO Y ENVÍO DE ILUMINACIÓN AL SHADER
        // ------------------------------------------------------------
        glUniform3fv(lightPositionsLoc, kNumCeilingLights, &ceilingLights[0].x);

        glm::vec3 currentLightColor(0.0f);
        switch (lightColorMode)
        {
        case 0: // Apagado general
            currentLightColor = glm::vec3(0.0f);
            break;
        case 1: // Blanco Cálido de Estadio
            currentLightColor = glm::vec3(0.42f * 1.00f, 0.42f * 0.97f, 0.42f * 0.92f);
            break;
        case 2: // Azul Neón
            currentLightColor = glm::vec3(0.12f, 0.35f, 0.95f);
            break;
        case 3: // Rojo Escarlata
            currentLightColor = glm::vec3(0.95f, 0.15f, 0.18f);
            break;
        case 4: // MODO REFLECTOR CÓNICO DESTACADO: Penumbra tenue en techo para resaltar el cono
            currentLightColor = glm::vec3(0.05f, 0.08f, 0.18f);
            break;
        }

        glUniform3f(lightColorLoc, currentLightColor.x, currentLightColor.y, currentLightColor.z);

        // Luz direccional
        glUniform3f(dirLightDirectionLoc, dirLightDirection.x, dirLightDirection.y, dirLightDirection.z);
        const glm::vec3 dirColorNow = dirLightOn ? dirLightColor : glm::vec3(0.0f);
        glUniform3f(dirLightColorLoc, dirColorNow.x, dirColorNow.y, dirColorNow.z);

        // Reflector Cónico (Spotlight): Brillo máximo (2.5x) de forma automática en el Modo 4
        glUniform3f(spotPositionLoc, spotPosition.x, spotPosition.y, spotPosition.z);
        glUniform3f(spotDirectionLoc, spotDirection.x, spotDirection.y, spotDirection.z);
        glUniform1f(spotInnerCosLoc, spotInnerCos);
        glUniform1f(spotOuterCosLoc, spotOuterCos);

        const bool spotActive = spotLightOn || (lightColorMode == 4);
        const glm::vec3 spotColorNow = spotActive ?
            (lightColorMode == 4 ? glm::vec3(2.5f, 2.3f, 2.0f) : spotColor) : glm::vec3(0.0f);

        glUniform3f(spotColorLoc, spotColorNow.x, spotColorNow.y, spotColorNow.z);

        // ------------------------------------------------------------
        // ACTUALIZACIÓN DE ESTADO DEL PARTIDO (Pelota, Jugadores y Marcador)
        // ------------------------------------------------------------
        const glm::vec4 ballState = match.ball(activeMatchTime);
        const glm::vec3 ballPos(ballState);

        const Anim::PlayerPose poseRojo = match.pose(0, activeMatchTime);
        const Anim::PlayerPose poseAzul = match.pose(1, activeMatchTime);
        const Anim::ScoreEvent score = match.scoreAt(activeMatchTime);

        // ------------------------------------------------------------
        // PASADA DE RENDER 1: Escenario Estático y Utilería
        // ------------------------------------------------------------
        glUniform3f(objectColorLoc, 1.0f, 1.0f, 1.0f);
        for (const ModelInstance& instance : sceneInstances)
        {
            if (instance.modelId == MODEL_ESPECTADORES_CUERPO ||
                instance.modelId == MODEL_ESPECTADORES_BRAZOS ||
                instance.modelId == MODEL_JUGADOR_ROJO_CUERPO ||
                instance.modelId == MODEL_JUGADOR_ROJO_BRAZO ||
                instance.modelId == MODEL_JUGADOR_ROJO_CABEZA ||
                instance.modelId == MODEL_JUGADOR_AZUL_CUERPO ||
                instance.modelId == MODEL_JUGADOR_AZUL_BRAZO ||
                instance.modelId == MODEL_JUGADOR_AZUL_CABEZA ||
                instance.modelId == MODEL_PALETA ||
                instance.modelId == MODEL_PELOTA)
                continue;

            glm::mat4 modelTransform(1.0f);

            // Desactivar Face Culling para renderizar el interior del Recinto
            if (instance.modelId == MODEL_RECINTO)
            {
                glDisable(GL_CULL_FACE);
                modelTransform = glm::translate(modelTransform, instance.position);
                modelTransform = glm::rotate(modelTransform, glm::radians(instance.rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
                modelTransform = glm::scale(modelTransform, glm::vec3(instance.scale));
                glUniformMatrix4fv(modelLocation, 1, GL_FALSE, &modelTransform[0][0]);
                models[instance.modelId].Draw(shader);
                glEnable(GL_CULL_FACE);
                continue;
            }

            modelTransform = createModelTransform(models[instance.modelId], instance);
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, &modelTransform[0][0]);
            models[instance.modelId].Draw(shader);
        }

        // ------------------------------------------------------------
        // PASADA DE RENDER 1.5: Marcador Digital de Puntaje
        // ------------------------------------------------------------
        {
            float flash = (score.scorer >= 0 && activeMatchTime >= score.t) ?
                std::exp(-(activeMatchTime - score.t) * 2.2f) : 0.0f;

            glUniform3f(objectColorLoc, 1.0f, 1.0f, 1.0f);
            marcador.Draw(shader, modelLocation, marcadorTransform, score.rojo, score.azul,
                (score.scorer == 0) ? flash : 0.0f, (score.scorer == 1) ? flash : 0.0f);
        }

        // ------------------------------------------------------------
        // PASADA DE RENDER 2: Jugadores Articulados (Skinning)
        // ------------------------------------------------------------
        drawPlayer(jugadorRojo, poseRojo, MODEL_JUGADOR_ROJO_CUERPO, MODEL_JUGADOR_ROJO_BRAZO, MODEL_JUGADOR_ROJO_CABEZA, glm::vec3(0.95f, 0.25f, 0.25f));
        drawPlayer(jugadorAzul, poseAzul, MODEL_JUGADOR_AZUL_CUERPO, MODEL_JUGADOR_AZUL_BRAZO, MODEL_JUGADOR_AZUL_CABEZA, glm::vec3(0.25f, 0.45f, 0.95f));

        // ------------------------------------------------------------
        // PASADA DE RENDER 3: Pelota de Tenis de Mesa
        // ------------------------------------------------------------
        if (ballState.w > 0.01f)
        {
            glm::mat4 ballTransform(1.0f);
            ballTransform = glm::translate(ballTransform, ballPos);
            ballTransform = glm::rotate(ballTransform, currentFrame * 30.0f, glm::normalize(glm::vec3(0.2f, 0.0f, 1.0f)));
            ballTransform = glm::scale(ballTransform, glm::vec3(ballState.w));
            ballTransform = glm::translate(ballTransform, glm::vec3(0.0f, -Anim::kBallRadius, 0.0f));

            glUniform3f(objectColorLoc, 1.0f, 1.0f, 1.0f);
            setMat(modelLocation, ballTransform);
            models[MODEL_PELOTA].Draw(shader);
        }

        // ------------------------------------------------------------
        // PASADA DE RENDER 4: Espectadores con Animación y Ropa Colorida
        // ------------------------------------------------------------
        glDisable(GL_CULL_FACE);
        const glm::vec3 bodyCenter = models[MODEL_ESPECTADORES_CUERPO].GetCenter();
        const float bodyMinY = models[MODEL_ESPECTADORES_CUERPO].GetMinY();

        // Paleta de colores para variaciones de vestimenta del público
        std::vector<glm::vec3> ropaColores = {
            glm::vec3(0.20f, 0.45f, 0.90f), glm::vec3(0.90f, 0.25f, 0.25f),
            glm::vec3(0.25f, 0.80f, 0.35f), glm::vec3(0.95f, 0.75f, 0.15f),
            glm::vec3(0.70f, 0.30f, 0.85f), glm::vec3(0.90f, 0.50f, 0.20f)
        };

        for (const ModelInstance& instance : sceneInstances)
        {
            if (instance.modelId != MODEL_ESPECTADORES_CUERPO && instance.modelId != MODEL_ESPECTADORES_BRAZOS)
                continue;

            // Variación de movimiento basada en la posición
            float phaseOffset = instance.position.x * 2.3f + instance.position.z * 3.7f;
            float jumpY = std::abs(std::sin(currentFrame * 2.5f + phaseOffset)) * 0.015f;

            glm::mat4 modelTransform = createModelTransformCustom(bodyCenter, bodyMinY, instance);
            modelTransform = glm::translate(modelTransform, glm::vec3(0.0f, jumpY, 0.0f));

            // Animación de alzamiento de brazos al celebrar
            if (instance.modelId == MODEL_ESPECTADORES_BRAZOS)
            {
                float waveFactor = (std::sin(currentFrame * 3.2f + phaseOffset) * 0.5f + 0.5f);
                float armUpRotation = glm::radians(40.0f + waveFactor * 70.0f);
                const glm::vec3 spectatorShoulderPivot(0.0f, 1.12f, 0.02f);

                modelTransform = glm::translate(modelTransform, spectatorShoulderPivot);
                modelTransform = glm::rotate(modelTransform, -armUpRotation, glm::vec3(1.0f, 0.0f, 0.0f));
                modelTransform = glm::translate(modelTransform, -spectatorShoulderPivot);
            }

            // Asignación de color según coordenadas
            float posHash = std::abs(std::sin(instance.position.x * 12.9898f + instance.position.z * 78.233f) * 43758.5453f);
            int colorIdx = static_cast<int>(posHash) % static_cast<int>(ropaColores.size());
            glm::vec3 colorRopa = ropaColores[colorIdx];

            glUniform3f(objectColorLoc, colorRopa.x, colorRopa.y, colorRopa.z);
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, &modelTransform[0][0]);
            models[instance.modelId].Draw(shader);
        }

        glEnable(GL_CULL_FACE);

        // Intercambio de buffers de pantalla y procesamiento de eventos
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 8. LIBERACIÓN DE RECURSOS
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}