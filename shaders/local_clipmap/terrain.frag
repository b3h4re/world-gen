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
    vec3 normal = normalize(cross(dFdx(fragmentPosition), dFdy(fragmentPosition)));
    vec3 lightDirection = normalize(vec3(0.35, 0.80, 0.45));
    float normalLight = 0.55 + 0.45 * abs(dot(normal, lightDirection));
    float windingLight = gl_FrontFacing ? 1.0 : 0.45;
    vec3 cacheColor = mix(vec3(1.0, 0.04, 0.02), push.levelColor.rgb,
        push.cacheValidity);
    outColor = vec4(cacheColor * normalLight * windingLight, 1.0);
}
