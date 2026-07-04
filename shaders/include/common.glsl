float lerp(float a, float b, float c) {
    return a + c * (b - a);
}

float defaultPerlinInterp(float t) {
    return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
}

float minkowskiDistance(vec2 v1, vec2 v2, float p) {
    return pow(
        pow(abs(v1.x - v2.x), p) + pow(abs(v1.y - v2.y), p),
        1.0F / p
    );
}

float lowPassFilter(int x, vec3 params) {
    if (x == -1) {
        return params.x;
    }
    if (x == 0) {
        return params.y;
    }
    return params.z;
}
