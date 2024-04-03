#version 450

layout(push_constant, std430) uniform PushConstants {
    vec4 mat_ambient;
    vec4 mat_diffuse;
    vec4 mat_specular;
    float mat_shininess;
    vec3 light_dir;
} registers;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec4 eye;

layout(location = 0) out vec4 out_color;

void main(void) {
   vec4 spec = vec4(0.0);
   vec3 n = normalize(normal);
   vec3 e = normalize(vec3(eye));
   float intensity = max(dot(n, registers.light_dir), 0.0);
   if (intensity > 0.0) {
       vec3 h = normalize(registers.light_dir + e);
       float intSpec = max(dot(h, n), 0.0);
       spec = registers.mat_specular * pow(intSpec, registers.mat_shininess);
   }
   out_color = max(intensity * registers.mat_diffuse + spec, registers.mat_ambient);
}
