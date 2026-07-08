#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;

layout(location = 0) out float fragHeight;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 color;
} push;

void main() {
    gl_Position = push.model * vec4(position, 1.0);
    fragHeight = height;
}
