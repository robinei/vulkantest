#version 450

layout(push_constant, std430) uniform PushConstants {
    mat4 m_pvm;
} registers;

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 texcoord;

void main(void) {
    gl_Position = registers.m_pvm * vec4(in_pos, 1.0);
    texcoord = in_pos;
}
