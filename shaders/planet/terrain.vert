#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;

layout(location = 0) out float fragmentHeight;

layout(push_constant) uniform Push {
    mat4 projectionView;
    mat4 model;
} push;

void main() {
    gl_Position = push.projectionView * push.model * vec4(position, 1.0);
    fragmentHeight = height;
}
