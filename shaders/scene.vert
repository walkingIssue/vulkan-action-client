#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pc;

void main()
{
    gl_Position = pc.viewProjection * vec4(inPosition, 1.0);
    outNormal = inNormal;
    outColor = inColor;
}
