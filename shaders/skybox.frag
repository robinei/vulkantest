#version 450

layout(binding = 0) uniform samplerCube tex;

layout(location = 0) in vec3 texcoord;

layout(location = 0) out vec4 out_color;

void main(void) {
    out_color = texture(tex, texcoord);
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
}
