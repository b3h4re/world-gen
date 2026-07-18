#version 450

layout(location = 0) in float fragmentHeight;
layout(location = 1) in vec2 fragmentLocalPositionMeters;
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

void main() {
    if (ubo.hybridParams.x > 0.5) {
        float coverage = hybridSquareCoverage(
            fragmentLocalPositionMeters,
            ubo.localCoverage);
        if (!hybridKeepsGlobal(coverage, ivec2(gl_FragCoord.xy))) {
            discard;
        }
    }
    float maxHeight = ubo.heightParams.y;
    float minHeight = ubo.heightParams.x;
    float delta = maxHeight - minHeight;
    float height = (fragmentHeight - minHeight) / delta;

    outColor = texture(uHeightColorMap, vec2(height, 0.5));
}
