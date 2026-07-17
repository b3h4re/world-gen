#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out float fragmentHeight;

layout(push_constant) uniform Push {
    mat4 projectionView;
    vec3 relativeOrigin;
    float cacheValidity;
    vec4 levelColor;
} push;

void main() {
    vec3 cameraRelativePosition = push.relativeOrigin + position;
    gl_Position = push.projectionView * vec4(cameraRelativePosition, 1.0);
    fragmentPosition = cameraRelativePosition;
    fragmentHeight = height;
}
