#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;
layout(location = 2) in vec3 parentPosition;
layout(location = 3) in float parentHeight;

layout(location = 0) out float fragmentHeight;

layout(push_constant) uniform Push {
    mat4 projectionView;
    float terrainMorph;
} push;

void main() {
    vec3 morphedPosition = mix(parentPosition, position, push.terrainMorph);
    gl_Position = push.projectionView * vec4(morphedPosition, 1.0);
    fragmentHeight = mix(parentHeight, height, push.terrainMorph);
}
