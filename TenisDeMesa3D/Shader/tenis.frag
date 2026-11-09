#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 color;

uniform vec3 baseColor;
uniform vec3 objectColor;
uniform vec3 emissiveColor;          // Ke del .mtl (lamparas, marcador)
uniform int hasDiffuseTexture;
uniform sampler2D texture_diffuse1;

uniform vec3 viewPos;

// 12 luminarias del techo (posiciones en el mundo, enviadas desde C++)
const int NUM_LIGHTS = 12;
uniform vec3 lightPositions[NUM_LIGHTS];
uniform vec3 lightColor;             // color * intensidad de cada luminaria

// Luz DIRECCIONAL (rayos paralelos, como el sol: no tiene posicion ni atenuacion).
// Se apaga enviando dirLightColor = (0,0,0).
uniform vec3 dirLightDirection;      // hacia donde viajan los rayos
uniform vec3 dirLightColor;

// REFLECTOR / spot (cono de luz con borde suave). Se apaga con spotColor = (0,0,0).
uniform vec3 spotPosition;
uniform vec3 spotDirection;          // eje del cono (hacia donde apunta)
uniform float spotInnerCos;          // cos del angulo interior (luz plena)
uniform float spotOuterCos;          // cos del angulo exterior (la luz llega a 0)
uniform vec3 spotColor;

void main()
{
    vec3 albedo = baseColor;

    if (hasDiffuseTexture == 1)
    {
        albedo = texture(texture_diffuse1, TexCoords).rgb;
    }

    if (length(albedo) < 0.001)
    {
        albedo = vec3(1.0);
    }

    vec3 tint = vec3(1.0);

    // Si el material no tiene textura y su color base es blanco/gris (la camiseta en el .mtl),
    // se aplica el color dinamico de ropa (objectColor).
    // Si es piel o cabello (tonos oscuros o carne en el .mtl), se mantiene su color intacto.
    if (hasDiffuseTexture == 0 && length(objectColor) > 0.001)
    {
        if (baseColor.r > 0.7 && baseColor.g > 0.7 && baseColor.b > 0.7)
        {
            tint = objectColor;
        }
    }

    vec3 surface = albedo * tint;

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // El recinto se dibuja a doble cara: la normal siempre mira hacia el observador.
    if (dot(norm, viewDir) < 0.0)
    {
        norm = -norm;
    }

    // Luz ambiente hemisferica (algo mas clara hacia arriba) para que
    // las zonas alejadas de las luminarias no queden negras.
    vec3 ambient = mix(vec3(0.30), vec3(0.42), norm.y * 0.5 + 0.5);

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        vec3 toLight = lightPositions[i] - FragPos;
        float dist = length(toLight);
        vec3 lightDir = toLight / dist;

        // Caida con la distancia: mas luz bajo cada luminaria, mas suave hacia las esquinas.
        float attenuation = 1.0 / (1.0 + 0.10 * dist + 0.035 * dist * dist);

        float diff = max(dot(norm, lightDir), 0.0);
        diffuse += diff * lightColor * attenuation;

        // Brillo especular suave (reflejo de las luminarias en el suelo)
        vec3 halfway = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfway), 0.0), 48.0);
        specular += 0.10 * spec * lightColor * attenuation;
    }

    // ---- Luz direccional ----
    if (dot(dirLightColor, dirLightColor) > 0.0)
    {
        vec3 dirL = normalize(-dirLightDirection);
        float dd = max(dot(norm, dirL), 0.0);
        diffuse += dd * dirLightColor;

        vec3 halfwayD = normalize(dirL + viewDir);
        specular += 0.10 * pow(max(dot(norm, halfwayD), 0.0), 48.0) * dirLightColor;
    }

    // ---- Reflector (spot) ----
    if (dot(spotColor, spotColor) > 0.0)
    {
        vec3 toSpot = spotPosition - FragPos;
        float distSpot = length(toSpot);
        vec3 spotL = toSpot / distSpot;

        // theta = coseno del angulo entre el eje del cono y el rayo hacia el fragmento
        float theta = dot(spotL, normalize(-spotDirection));
        float cone = clamp((theta - spotOuterCos) / (spotInnerCos - spotOuterCos), 0.0, 1.0);
        cone = cone * cone * (3.0 - 2.0 * cone);

        float attSpot = 1.0 / (1.0 + 0.05 * distSpot + 0.012 * distSpot * distSpot);

        diffuse += max(dot(norm, spotL), 0.0) * spotColor * attSpot * cone;

        vec3 halfwayS = normalize(spotL + viewDir);
        specular += 0.14 * pow(max(dot(norm, halfwayS), 0.0), 48.0) * spotColor * attSpot * cone;
    }

    vec3 lit = surface * (ambient + diffuse) + specular + emissiveColor;

    color = vec4(lit, 1.0);
}
