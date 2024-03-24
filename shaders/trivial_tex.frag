#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 out_color;

void main(void) {
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
    out_color = texture(tex, uv);
}
