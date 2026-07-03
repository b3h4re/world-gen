#version 450

layout(location = 0) in float fragmentHeight;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uHeightColorMap;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;

    vec2 heightParams;
} ubo;


void main() {
    float maxHeight = ubo.heightParams.y;
    float minHeight = ubo.heightParams.x;
    float delta = maxHeight - minHeight;
    float offset = (maxHeight - minHeight) / 2.0F;
    float height = (fragmentHeight + offset) / delta;

    outColor = texture(uHeightColorMap, vec2(height, 0.5));
}
