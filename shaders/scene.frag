#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(inNormal);
    if (length(normal) < 0.001) {
        normal = vec3(0.0, 1.0, 0.0);
    }

    vec3 lightDirection = normalize(vec3(-0.35, 0.85, 0.45));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 color = inColor * (0.28 + diffuse * 0.72);
    outColor = vec4(color, 1.0);
}
