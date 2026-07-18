#version 450

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in float fragmentHeight;
layout(location = 2) in vec2 fragmentLocalPositionMeters;
layout(location = 0) out vec4 outColor;

#include "hybrid_coverage.glsl"

layout(set = 0, binding = 1) uniform sampler2D uHeightColorMap;

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
    vec3 relativeOrigin;
    float cacheValidity;
    vec4 levelColor;
} push;

void main() {
    if (ubo.hybridParams.x > 0.5) {
        float coverage = hybridSquareCoverage(
            fragmentLocalPositionMeters,
            ubo.localCoverage);
        if (!hybridKeepsLocal(coverage, ivec2(gl_FragCoord.xy))) {
            discard;
        }
        float heightDelta = ubo.heightParams.y - ubo.heightParams.x;
        float normalizedHeight =
            (fragmentHeight - ubo.heightParams.x) / heightDelta;
        outColor = texture(
            uHeightColorMap,
            vec2(normalizedHeight, 0.5));
        return;
    }
    vec3 normal = normalize(cross(dFdx(fragmentPosition), dFdy(fragmentPosition)));
    vec3 lightDirection = normalize(vec3(0.35, 0.80, 0.45));
    float normalLight = 0.55 + 0.45 * abs(dot(normal, lightDirection));
    float windingLight = gl_FrontFacing ? 1.0 : 0.45;
    vec3 cacheColor = mix(vec3(1.0, 0.04, 0.02), push.levelColor.rgb,
        push.cacheValidity);
    outColor = vec4(cacheColor * normalLight * windingLight, 1.0);
}
