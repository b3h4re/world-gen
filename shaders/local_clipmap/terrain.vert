#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in float height;
layout(location = 2) in vec2 localPositionMeters;

layout(location = 0) out vec3 fragmentPosition;
layout(location = 1) out float fragmentHeight;
layout(location = 2) out vec2 fragmentLocalPositionMeters;

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
    // {hybrid enabled, flatten amount, flat extent, curved extent}.
    vec4 hybridParams;
} ubo;

layout(push_constant) uniform Push {
    mat4 projectionView;
    vec3 relativeOrigin;
    float cacheValidity;
    vec4 levelColor;
} push;

void main() {
    vec3 presentedPosition = position;
    if (ubo.hybridParams.y > 0.0 && ubo.localFrameEastRadius.w > 0.0) {
        float planetRadiusMeters = ubo.localFrameEastRadius.w;
        vec3 tangentPosition =
            ubo.localFrameEastRadius.xyz *
                (localPositionMeters.x / planetRadiusMeters) +
            ubo.localFrameNorth.xyz *
                (localPositionMeters.y / planetRadiusMeters) +
            ubo.localFrameUp.xyz * (height / planetRadiusMeters);
        float centerMask = hybridPresentationCenterMask(
            localPositionMeters,
            vec2(ubo.localFrameNorth.w, ubo.localFrameUp.w),
            ubo.hybridParams);
        float presentationWeight = ubo.hybridParams.y * centerMask;
        presentedPosition = mix(
            position,
            tangentPosition,
            presentationWeight);
    }
    vec3 cameraRelativePosition = push.relativeOrigin + presentedPosition;
    gl_Position = push.projectionView * vec4(cameraRelativePosition, 1.0);
    // Fragment derivatives now describe the presented surface while height
    // remains the original spherical terrain sample.
    fragmentPosition = cameraRelativePosition;
    fragmentHeight = height;
    fragmentLocalPositionMeters = localPositionMeters;
}
