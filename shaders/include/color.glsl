#include "constants.glsl"

vec3 terrainColor(float height) {
    float re = (1 - height * height) / (1 + height * height);
    float im = 2 * height / (1 + height * height);
    float theta = atan(im, re);
    float H_ = 6 * (0.333f - 2 * theta * 0.333f / PI);

    float X = 1 - abs(mod(H_, 2) - 1);

    if (H_ < 1) {
        return vec3(1.0f, X, 0.0f);
    }

    if (H_ < 2) {
        return vec3(X, 1.0f, 0.0f);
    }

    if (H_ < 3) {
        return vec3(0.0f, 1.0f, X);
    }

    if (H_ < 4) {
        return vec3(0.0f, X, 1.0f);
    }

    if (H_ < 5) {
        return vec3(X, 0.0f, 1.0f);
    }

    return vec3(1.0f, 0.0f, X);
}


vec3 terrainBlackAndWhite(float height) {
    return vec3((height + 1) / 2);
}
