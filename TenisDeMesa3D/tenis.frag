#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3 viewPos;
uniform vec3 objectColor;
uniform int hasDiffuseTexture;
uniform sampler2D texture_diffuse1;
uniform vec3 baseColor;

// Focos de techo de alta intensidad
const int NUM_LIGHTS = 2;
uniform vec3 lightPositions[NUM_LIGHTS];
uniform vec3 lightColors[NUM_LIGHTS];

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

    vec3 tint = (length(objectColor) > 0.001) ? objectColor : vec3(1.0);
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Luz ambiental en 0.04 para mantener las esquinas y partes alejadas en oscuridad
    vec3 ambient = 0.04 * albedo * tint;
    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        vec3 lightVector = lightPositions[i] - FragPos;
        float dist = length(lightVector);
        vec3 lightDir = normalize(lightVector);

        // Caída de luz pronunciada para concentrar los haces en la cancha
        float attenuation = 1.0 / (1.0 + 0.12 * dist + 0.06 * (dist * dist));

        // Iluminación Difusa
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColors[i] * albedo * tint * attenuation;

        // Iluminación Especular Blinn-Phong
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
        vec3 specular = 0.5 * spec * lightColors[i] * attenuation;

        totalDiffuse += diffuse;
        totalSpecular += specular;
    }

    vec3 finalColor = ambient + totalDiffuse + totalSpecular;
    FragColor = vec4(finalColor, 1.0);
}