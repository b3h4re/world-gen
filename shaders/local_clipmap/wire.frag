#version 450

layout(location = 0) in vec3 fragmentPosition;
layout(location = 1) in float fragmentHeight;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 projectionView;
    vec3 relativeOrigin;
    float cacheValidity;
    vec4 levelColor;
} push;

void main() {
    vec3 cacheColor = mix(vec3(1.0, 0.04, 0.02), push.levelColor.rgb,
        push.cacheValidity);
    outColor = vec4(min(cacheColor * 1.25, vec3(1.0)), 1.0);
}
