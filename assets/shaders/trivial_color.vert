#version 450

layout(push_constant, std430) uniform PushConstants {
    uniform mat4 m_pvm;
} registers;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = registers.m_pvm * inPosition;
    outColor = inColor;
}
