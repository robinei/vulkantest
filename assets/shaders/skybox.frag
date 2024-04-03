#version 450

layout(set = 0, binding = 0) uniform sampler smp;
layout(set = 0, binding = 1) uniform textureCube tex;

layout(location = 0) in vec3 texcoord;

layout(location = 0) out vec4 out_color;

void main(void) {
    out_color = texture(samplerCube(tex, smp), texcoord);
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
}
