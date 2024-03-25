#version 450

layout(push_constant, std430) uniform PushConstants {
    uniform mat4 m_pvm;
} registers;

layout(location = 0) in vec4 in_pos;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;

void main(void) {
    uv = in_uv;
    color = in_color;
    gl_Position = registers.m_pvm * in_pos;
}
