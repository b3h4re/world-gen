#version 450

layout(location = 0) in float fragHeight;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
} push;

void main() {
    vec3 shadow = vec3(0.04, 0.18, 0.33);
    vec3 light = push.color.rgb;
    outColor = vec4(mix(shadow, light, clamp(fragHeight, 0.0, 1.0)), push.color.a);
}
