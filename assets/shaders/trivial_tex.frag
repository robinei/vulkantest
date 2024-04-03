#version 450

layout(set = 0, binding = 0) uniform sampler smp;
layout(set = 0, binding = 1) uniform texture2D tex;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 out_color;

void main(void) {
    //out_color = vec4(1.0, 1.0, 0.0, 1.0);
    out_color = texture(sampler2D(tex, smp), uv);
    //out_color = color;
}
