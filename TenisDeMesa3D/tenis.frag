
#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 color;

uniform vec3 baseColor;
uniform int hasDiffuseTexture;
uniform sampler2D texture_diffuse1;

void main()
{   
    vec3 albedo = baseColor;

    if (hasDiffuseTexture == 1)
    {
        albedo = texture(texture_diffuse1, TexCoords).rgb;
    }

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(vec3(-0.4, 1.0, 0.3));

    float diffuse = max(dot(norm, lightDir), 0.0);
    float illumination = 0.65 + 0.45 * diffuse;

    color = vec4(albedo * illumination, 1.0);
}
