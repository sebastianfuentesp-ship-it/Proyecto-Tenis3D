
#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// ---------------------------------------------------------------------------
// Skinning procedural (sin modificar los .obj): los huesos se mezclan segun la
// posicion del vertice en el espacio local de la malla.
//   skinMode 0 : sin skinning (todo el resto de la escena)
//   skinMode 1 : cuerpo del jugador
//                boneA = piernas, boneB = torso, boneC = brazo libre
//                skinP0 = (yInicioTorso, yFinTorso, ejeTorsoX, ladoBrazoLibre)
//   skinMode 2 : brazo con raqueta
//                boneA = brazo superior, boneB = antebrazo
//                skinP0 = (codo.xyz, semiAnchoMezcla), skinP1.xyz = eje codo->mano
// ---------------------------------------------------------------------------
uniform int skinMode;
uniform mat4 boneA;
uniform mat4 boneB;
uniform mat4 boneC;
uniform vec4 skinP0;
uniform vec4 skinP1;

void main()
{
    vec4 p = vec4(position, 1.0);
    vec3 n = normal;

    if (skinMode == 1)
    {
        float wTorso = smoothstep(skinP0.x, skinP0.y, position.y);
        float dx     = skinP0.w * (position.x - skinP0.z);
        float wFree  = smoothstep(0.17, 0.27, dx) * smoothstep(0.80, 0.92, position.y);

        vec4 pBody = mix(boneA * p, boneB * p, wTorso);
        p = mix(pBody, boneC * p, wFree);

        vec3 nBody = mix(mat3(boneA) * n, mat3(boneB) * n, wTorso);
        n = mix(nBody, mat3(boneC) * n, wFree);
    }
    else if (skinMode == 2)
    {
        float wFore = smoothstep(-skinP0.w, skinP0.w, dot(position - skinP0.xyz, skinP1.xyz));
        p = mix(boneA * p, boneB * p, wFore);
        n = mix(mat3(boneA) * n, mat3(boneB) * n, wFore);
    }

    vec4 posicionMundo = model * p;

    FragPos = vec3(posicionMundo);
    Normal = mat3(transpose(inverse(model))) * n;
    TexCoords = texCoords;

    gl_Position = projection * view * posicionMundo;
}
