#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 color;

uniform vec3 baseColor;
uniform vec3 objectColor;
uniform int hasDiffuseTexture;
uniform sampler2D texture_diffuse1;

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

    // Si la sub-malla tiene textura de piel/cabello o un color base distinto de blanco,
    // se mantiene su color original sin aplicar el tinte.
    vec3 tint = objectColor;

    // Detectar si la parte renderizada no es la ropa (piel, cabello o textura cargada)
    bool esPielOCabello = (hasDiffuseTexture == 1) || 
                          (baseColor.r > 0.6 && baseColor.g > 0.4 && baseColor.b < 0.6);

    if (esPielOCabello)
    {
        tint = vec3(1.0, 1.0, 1.0); // Mantiene piel y cabello intactos
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(vec3(-0.4, 1.0, 0.3));

    float diffuse = max(dot(norm, lightDir), 0.0);
    float illumination = 0.65 + 0.45 * diffuse;

    color = vec4(albedo * illumination * tint, 1.0);
}