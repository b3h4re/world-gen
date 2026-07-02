#version 450
#include "color.glsl"

layout(location = 0) in float fragmentHeight;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;

    uint colorFuncID;
} ubo;

void main() {
    vec3 color = vec3(0.5);
    if (ubo.colorFuncID == 0) {
        color = terrainColor(fragmentHeight);
    }
    if (ubo.colorFuncID == 1) {
        color = terrainBlackAndWhite(fragmentHeight);
    }
    outColor = vec4(color, 1.0);
}
