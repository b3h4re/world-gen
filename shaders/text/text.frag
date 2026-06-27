#version 450

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec3 fragColor;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) out vec4 outColor;

void main() {
    float alpha = texture(fontAtlas, fragUv).r;
    outColor = vec4(fragColor, alpha);
}
