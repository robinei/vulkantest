#version 450

layout(push_constant, std430) uniform PushConstants {
   mat4 m_pvm;
   mat4 m_vm;
   mat3 m_normal;
} registers;

layout(location = 0) in vec4 in_pos;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec4 eye;

void main(void) {
   normal = normalize(registers.m_normal * in_normal);
   eye = -(registers.m_vm * in_pos);
   gl_Position = registers.m_pvm * in_pos;
}
