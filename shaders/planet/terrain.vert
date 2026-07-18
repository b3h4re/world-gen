#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;
layout(location = 2) in vec3 parentPosition;
layout(location = 3) in float parentHeight;

layout(location = 0) out float fragmentHeight;
layout(location = 1) out vec2 fragmentLocalPositionMeters;

#include "hybrid_coverage.glsl"

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec2 heightParams;
    vec4 localFrameEastRadius;
    vec4 localFrameNorth;
    vec4 localFrameUp;
    vec4 localCoverage;
    vec4 hybridParams;
} ubo;

layout(push_constant) uniform Push {
    mat4 projectionView;
    vec3 relativePatchOrigin;
    float terrainMorph;
    vec3 globalPatchOrigin;
    float padding;
} push;

void main() {
    vec3 morphedPosition = mix(parentPosition, position, push.terrainMorph);
    gl_Position = push.projectionView *
        vec4(push.relativePatchOrigin + morphedPosition, 1.0);
    fragmentHeight = mix(parentHeight, height, push.terrainMorph);
    vec3 globalPosition = push.globalPatchOrigin + morphedPosition;
    fragmentLocalPositionMeters = hybridTangentPosition(
        globalPosition,
        ubo.localFrameEastRadius.xyz,
        ubo.localFrameNorth.xyz,
        ubo.localFrameUp.xyz,
        ubo.localFrameEastRadius.w);
}
